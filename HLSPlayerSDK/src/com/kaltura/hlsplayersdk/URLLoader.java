package com.kaltura.hlsplayersdk;

import java.io.IOException;

import org.apache.http.Header;

import android.util.Log;

import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
import com.kaltura.hlsplayersdk.manifest.BaseManifestItem;
import com.loopj.android.http.*;

public class URLLoader extends AsyncHttpResponseHandler 
{	
	public BaseManifestItem manifestItem = null;
	public String uri;
	
	public URLLoader(DownloadEventListener eventListener, BaseManifestItem item)
	{
		Log.i("URLLoader.URLLoader()", "Constructing" );
		setDownloadEventListener( eventListener );
		manifestItem = item;
	}
	
	public void get(String url)
	{
		HLSSegmentCache.httpClient().get(url, this);
	}
	
	
	/////////////////////////////////////////////
	// Listener interface
	////////////////////////////////////////////
	
	public interface DownloadEventListener
	{
		public void onDownloadComplete(URLLoader loader, String response);
		public void onDownloadFailed(URLLoader loader, String response);
	}
	
	private DownloadEventListener mDownloadEventListener = null;
	
	public void setDownloadEventListener(DownloadEventListener listener)
	{
		mDownloadEventListener = listener;
	}

	//////////////////////////////////
	// Event Handlers
	//////////////////////////////////
	@Override
	public void onFailure(int statusCode, Header[] headers, byte[] responseBody, Throwable error) {
		final URLLoader thisLoader = this;
		final int sc = statusCode;
		
		if (mDownloadEventListener != null)
		{
			// Post back to main thread to avoid re-entrancy that breaks OkHTTP.
			PlayerViewController.GetInterfaceThread().getHandler().post(new Runnable()
			{
				@Override
				public void run() {
					mDownloadEventListener.onDownloadFailed(thisLoader, "Failure: " + sc);				
				}
			});
		}
	}

	@Override
	public void onSuccess(int statusCode, Header[] headers, byte[] responseData) {
		final URLLoader thisLoader = this;
		final String response = new String(responseData);
		
				
		if (mDownloadEventListener != null)
		{
			// Post back to main thread to avoid re-entrancy problems
			PlayerViewController.GetInterfaceThread().getHandler().post(new Runnable()
			{
				@Override
				public void run() {
					mDownloadEventListener.onDownloadComplete(thisLoader, response==null?"null" : response);
				}
			});
		}		
	}
}
