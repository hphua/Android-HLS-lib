package com.kaltura.hlsplayersdk.cache;

import org.apache.http.Header;

import android.util.Log;

import com.loopj.android.http.BinaryHttpResponseHandler;

public class SegmentBinaryResponseHandler extends BinaryHttpResponseHandler 
{
	public String cacheKey;
	
	public SegmentBinaryResponseHandler(String uri)
	{
		super( new String[] { ".*" } );
		cacheKey = uri;
	}
	
	@Override
	public void onStart() 
	{
		Log.i("HLS Cache", "Request for " + cacheKey + " started...");		
	}
	
	@Override
	public void onSuccess(int statusCode, Header[] headers, byte[] binaryData)
	{
		// Stuff into the cache.
		Log.i("HLS Cache", "Got segment " + statusCode + " for " + getRequestURI().toString() + " size=" + binaryData.length);
		HLSSegmentCache.store(cacheKey, binaryData);
	}
	
	public void onFailure(int statusCode, Header[] headers, byte[] binaryData, Throwable error)
	{
		Log.i("HLS Cache", "Got segment failure " + statusCode + " for " + cacheKey + ": " + error.toString());
		HLSSegmentCache.store(cacheKey, new byte[0]);
	}
}
