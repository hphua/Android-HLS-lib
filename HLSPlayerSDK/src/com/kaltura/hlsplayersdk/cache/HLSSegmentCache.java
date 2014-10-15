package com.kaltura.hlsplayersdk.cache;

import java.util.Collection;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.loopj.android.http.*;

//import com.squareup.okhttp.ConnectionPool;
//import com.squareup.okhttp.OkHttpClient;
//import com.squareup.okhttp.Request;

public class HLSSegmentCache 
{	
	protected static long targetSize = 16*1024*1024; // 16mb segment cache.
	protected static long minimumExpireAge = 5000; // Keep everything touched in last 5 seconds.
	
	protected static Map<String, SegmentCacheEntry> segmentCache = null;
	public static AsyncHttpClient asyncHttpClient = new AsyncHttpClient();
	public static AsyncHttpClient syncHttpClient = new SyncHttpClient();
	
	public static AsyncHttpClient httpClient()
	{
		if (Looper.myLooper() == null)
			return syncHttpClient;
		return asyncHttpClient;
	}
	
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
			
			// Issue HTTP request.
	
			Thread t = new Thread()		
			{		
				public void run() {		
					httpClient().get(sce.uri, new SegmentBinaryResponseHandler(sce));						
					//sce.request = client.get(sce.uri, new SegmentBinaryResponseHandler(segmentUri));						
				}		
			};		
			t.start();		
			 						
			 			
			
//			Request request = new Request.Builder()
//		      .url(segmentUri)
//		      .build();
//			httpClient.newCall(request).enqueue(sce);
			

			segmentCache.put(segmentUri, sce);		
			return sce;
		}
	}
	
	static public void store(String segmentUri, byte[] data)
	{
		SegmentCacheEntry sce = null;
		synchronized (segmentCache)
		{
			Log.i("HLS Cache", "Storing result of " + data.length + " for " + segmentUri);
			
			// Look up the cache entry.
			sce = segmentCache.get(segmentUri);
			if(sce == null)
			{
				Log.e("HLS Cache", "Lost entry for " + segmentUri + "!");
				return;
			}
			
			// Store the data.
			sce.data = data;
						
			// All done!
			sce.lastTouchedMillis = System.currentTimeMillis();
			//sce.notifySegmentCached();
			sce.running = false;
		}
		
		sce.notifySegmentCached();

		expire();
	}
	
	static protected void initialize()
	{
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
	 * @param cryptoId
	 */
	static public void precache(String segmentUri, int cryptoId)
	{
		initialize();
		populateCache(segmentUri);

		SegmentCacheEntry sce = segmentCache.get(segmentUri);
		sce.setCryptoHandle(cryptoId);
	}
	
	/**
	 * Hint we will soon be wanting data from this segment and that we should 
	 * initiate the download.
	 * 
	 * @param segmentUri
	 * @param cryptoId
	 * @param SegmentCachedListener
	 */
	static public void precache(final String segmentUri, int cryptoId, final SegmentCachedListener segmentCachedListener, Handler callbackHandler )
	{
		precache(segmentUri, cryptoId);
		synchronized (segmentCache)
		{
			SegmentCacheEntry sce = segmentCache.get(segmentUri);
			sce.registerSegmentCachedListener(segmentCachedListener, callbackHandler);
			
			if (!sce.running)
				sce.notifySegmentCached();
				
		}
	}
	
	/**
	 * Cancels all cache event notifications for a particular cache entry.
	 * 
	 * @param segmentUri
	 */
	static public void cancelCacheEvent(String segmentUri)
	{
		initialize();
		synchronized (segmentCache)
		{
			SegmentCacheEntry sce = segmentCache.get(segmentUri);
			if (sce != null) sce.registerSegmentCachedListener(null, null);
		}
	}
	
	static public void cancelAllCacheEvents()
	{
		initialize();
		synchronized (segmentCache)
		{
			// Get all the values in the set.
			Collection<SegmentCacheEntry> values = segmentCache.values();

			for(SegmentCacheEntry v : values)
				v.registerSegmentCachedListener(null, null);
		}
	}
	
	
	/**
	 * Return the size of a downloaded segment. Blocking.
	 */
	static public long getSize(String segmentUri)
	{
		initialize();

		Log.i("HLS Cache", "Querying size of " + segmentUri);
		SegmentCacheEntry sce = segmentCache.get(segmentUri);
		if (sce == null)
		{
			// Do we have a cache entry for the segment? Populate if it doesn't exist.
			sce = populateCache(segmentUri);
		}
		waitForLoad(sce);
		if(sce.forceSize != -1)
			return sce.forceSize;
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
	
	static public String readFileAsString(String segmentUri)
	{
		initialize();
		
		long size = getSize(segmentUri);
		ByteBuffer buffer = ByteBuffer.allocate((int)size);
		read(segmentUri, 0, size, buffer);
		return new String(buffer.array(), Charset.forName("UTF-8"));
	}

final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();
public static String bytesToHex(ByteBuffer bytes) {
    char[] hexChars = new char[bytes.capacity() * 2];
    for ( int j = 0; j < bytes.capacity(); j++ ) {
        int v = bytes.get(j) & 0xFF;
        hexChars[j * 2] = hexArray[v >>> 4];
        hexChars[j * 2 + 1] = hexArray[v & 0x0F];
    }
    return new String(hexChars);
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
		boolean adjusted = false;

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
				Log.i("HLS Cache", "Adjusting size to " + newSize + " from " + size + " offset=" + offset);
				size = newSize;
				adjusted = true;
			}
			
			if(size < 0)
			{
				Log.i("HLS Cache", "Couldn't return any bytes.");
				return 0;
			}

			// Ensure decrypted.
			sce.ensureDecryptedTo(offset + size);

			// If we have decrypted to the end, look for padding and adjust length.
			if(sce.isFullyDecrypted() && sce.hasCrypto() && sce.forceSize == -1)
			{
				// Look for padding.
				byte padByte = sce.data[sce.data.length - 1];

				boolean isPadded = true;
				for(int i=sce.data.length-padByte; i<sce.data.length; i++)
				{
					if(sce.data[i] == padByte)
						continue;

					isPadded = false;
					break;
				}

				if(isPadded)
				{
					// Note new size.
					sce.forceSize = sce.data.length - padByte;
					Log.i("HLS Cache", "Forcing segment size to " + sce.forceSize);
				}
			} 
			
			// Truncate length based on forced size.
			if(sce.forceSize != -1)
			{
				if(offset + size >= sce.forceSize)
				{
					size = sce.forceSize - offset;
					Log.i("HLS Cache", "Truncating size due to padding to " + size);
				}
			}
			
			// Copy the available bytes.
			output.put(sce.data, (int)offset, (int)size);
			
			if(adjusted)
			{
				try
				{
					Log.i("HLS Cache", "Saw bytes: " + bytesToHex(output));
				}
				catch(Exception e)
				{
					Log.i("HLS Cache", "Failed to dump hex bytes");
				}
			}

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
