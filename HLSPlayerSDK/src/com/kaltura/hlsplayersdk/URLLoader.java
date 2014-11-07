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
		uri = url;
		Log.i("URLLoader", "Getting: " + uri);
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
		if (mDownloadEventListener != null && listener != null)
		{
			Log.e("URLLoader.setDownloadEventListener", "Tried to change the downloadEventListener for " + uri);
		}
		else
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
		
		Log.i("URLLoader.success", "Received: " + uri);
		final URLLoader thisLoader = this;
		//if (responseData.length > 16 * 1024)
		{
			
			for (int i = 0; i < headers.length; ++i)
			{
				Log.i("URLLoader.success", "Header: " + headers[i].getName() + ": " + headers[i].getValue());
			}
		}

		if (mDownloadEventListener == null) return;

		if (uri.lastIndexOf(".m3u8") == uri.length() - 5)
		{
			for (int i = 0; i < headers.length; ++i)
			{
				if (headers[i].getName().equals("Content-Type") && headers[i].getValue().contains("mpegurl") == false)
				{
					onFailure(statusCode, headers, responseData, null);
					return;
				}
			}
		}
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
