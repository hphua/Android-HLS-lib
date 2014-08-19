package com.kaltura.hlsplayersdk.cache;

import java.util.Collection;
import java.nio.ByteBuffer;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import android.util.Log;

import com.loopj.android.http.AsyncHttpClient;

public class HLSSegmentCache 
{	
	protected static long targetSize = 16*1024; // 16mb segment cache.
	protected static long minimumExpireAge = 5000; // Keep everything touched in last 5 seconds.
	
	protected static Map<String, SegmentCacheEntry> segmentCache = null;
	public static AsyncHttpClient client = null;
	
	static public SegmentCacheEntry populateCache(final String segmentUri)
	{
		synchronized (segmentCache)
		{
			// Hit the cache first.
			SegmentCacheEntry existing = segmentCache.get(segmentUri);
			if(existing != null)
			{
				existing.lastTouchedMillis = System.currentTimeMillis();
				return existing;
			}
			
			// Populate a cache entry and initiate the request.
			Log.i("HLS Cache", "Miss on " + segmentUri + ", populating..");
			final SegmentCacheEntry sce = new SegmentCacheEntry();
			sce.uri = segmentUri;
			sce.running = true;
			sce.lastTouchedMillis = System.currentTimeMillis();
			Thread t = new Thread()
			{
				public void run() {
					sce.request = client.get(sce.uri, new SegmentBinaryResponseHandler(segmentUri));				
				}
			};
			t.start();
			
			segmentCache.put(segmentUri, sce);		
			return sce;
		}
	}
	
	static public void store(String segmentUri, byte[] data)
	{
		synchronized (segmentCache)
		{
			Log.i("HLS Cache", "Storing result of " + data.length + " for " + segmentUri);
			
			// Look up the cache entry.
			SegmentCacheEntry sce = segmentCache.get(segmentUri);
			if(sce == null)
			{
				Log.e("HLS Cache", "Lost entry for " + segmentUri + "!");
				return;
			}
			
			// Store the data.
			sce.data = data;
			
			// Drop the request
			sce.request = null;
			
			// All done!
			sce.lastTouchedMillis = System.currentTimeMillis();
			sce.running = false;
		}

		expire();
	}
	
	static protected void initialize()
	{
		if(client == null)
		{		
			Log.i("HLS Cache", "Initializing loopj http client.");
			client = new AsyncHttpClient();
		}

		if(segmentCache == null)
		{
			Log.i("HLS Cache", "Initializing concurrent hash map.");
			segmentCache = new ConcurrentHashMap<String, SegmentCacheEntry>();
		}
	}
	
	/**
	 * Hint we will soon be wanting data from this segment and that we should 
	 * initiate the download.
	 * 
	 * @param segmentUri
	 */
	static public void precache(String segmentUri)
	{
		initialize();
		populateCache(segmentUri);
	}
	
	static public long getSize(String segmentUri)
	{
		Log.i("HLS Cache", "Querying size of " + segmentUri);
		SegmentCacheEntry sce = segmentCache.get(segmentUri);
		waitForLoad(sce);
		return sce.data.length;
	}
	
	static private void waitForLoad(SegmentCacheEntry sce)
	{
		// Wait for data, if required...
		if(!sce.running)
			return;
		
		Log.i("HLS Cache", "Waiting on request.");
		long timerStart = System.currentTimeMillis();

		while(sce.running)
		{
			try {
				Thread.sleep(30);
				Thread.yield();
			} catch (InterruptedException e) {
				// Don't care.
			}
		}
		
		long timerElapsed = System.currentTimeMillis() - timerStart;
		Log.i("HLS Cache", "Request finished, " + (sce.data.length/1024) + "kb in " + timerElapsed + "ms");			
	}
	
	/**
	 * Read from segment and return bytes read + output.
	 * @param segmentUri URI identifying the segment.
	 * @param offset Offset into the segment.
	 * @param size Number of bytes to read.
	 * @param output Array pre-sized to at least size, to which data is written.
	 * @return Bytes read.
	 */
	static public long read(String segmentUri, long offset, long size, ByteBuffer output)
	{
		//Log.i("HLS Cache", "Reading " + segmentUri + " offset=" + offset + " size=" + size + " output.capacity()=" + output.capacity());
		
		initialize();
		
		// Do we have a cache entry for the segment? Populate if it doesn't exist.
		SegmentCacheEntry sce = populateCache(segmentUri);
		
		// Sanity check.
		if(sce == null)
		{
			Log.e("HLS Cache", "Failed to populate cache! Aborting...");
			return 0;
		}
		
		waitForLoad(sce);
		
		synchronized(segmentCache)
		{
			// How many bytes can we serve?
			if(offset + size > sce.data.length)
			{
				long newSize = sce.data.length - offset;
				Log.i("HLS Cache", "Adjusting size to " + newSize + " from " + size);
				size = newSize;
			}
			
			if(size < 0)
			{
				Log.i("HLS Cache", "Couldn't return any bytes.");
				return 0;
			}
			
			// Copy the available bytes.
			output.put(sce.data, (int)offset, (int)size);
			
			// Return how much we read.
			return size;			
		}
	}
	
	/**
	 * We only have finite memory; evict segments when we exceed a maximum size.
	 */
	static public void expire()
	{
		synchronized (segmentCache)
		{
			// Get all the values in the set.
			Collection<SegmentCacheEntry> values = segmentCache.values();

			// First, determine total size.
			long totalSize = 0;
			for(SegmentCacheEntry v : values)
				if(v.data != null)
					totalSize += v.data.length;
			
			Log.i("HLS Cache", "size=" + (totalSize/1024) + "kb  threshold=" + (targetSize/1024) + "kb");
			
			// If under threshold, we're done.
			if(totalSize <= targetSize)
				return;
			
			// Otherwise, find the oldest segment.
			long oldestTime = System.currentTimeMillis();
			SegmentCacheEntry oldestSce = null;
			for(SegmentCacheEntry v : values)
			{
				if(v.lastTouchedMillis >= oldestTime)
					continue;
				
				oldestSce = v;
				oldestTime = v.lastTouchedMillis;
			}
			
			long entryAge = System.currentTimeMillis() - oldestTime;
			if(entryAge < minimumExpireAge)
			{
				Log.i("HLS Cache", "Tried to purge segment that is less than " + minimumExpireAge/1000 + " seconds old. Ignoring... (" + oldestSce.uri + ", " + entryAge/1000 + ")");
				return;
			}
			
			// We're over cache target, delete that one.
			Log.i("HLS Cache", "Purging " + oldestSce.uri + ", freeing " + (oldestSce.data.length/1024) + "kb, age " + (entryAge/1000) + "sec");
			segmentCache.remove(oldestSce.uri);
		}
	}
}
