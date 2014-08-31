package com.kaltura.hlsplayersdk;

import java.io.IOException;

import android.util.Log;

import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
import com.kaltura.hlsplayersdk.manifest.BaseManifestItem;
import com.squareup.okhttp.Callback;
import com.squareup.okhttp.Request;
import com.squareup.okhttp.Response;

public class URLLoader implements Callback 
{
	//private static AsyncHttpClient httpClient = new AsyncHttpClient();
	
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
		//httpClient.get(url, this);
		
		// OK path
		uri = url;
		Request request = new Request.Builder()
	      .url(uri)
	      .build();
		HLSSegmentCache.httpClient.newCall(request).enqueue(this);
	}
	
	public String getRequestURI()
	{
		return uri;
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

	// OkHttp methods.
	@Override
	public void onFailure(Request arg0, IOException arg1) {
		final URLLoader thisLoader = this;
		
		if (mDownloadEventListener != null)
		{
			// Post back to main thread to avoid re-entrancy that breaks OkHTTP.
			PlayerViewController.currentController.post(new Runnable()
			{
				@Override
				public void run() {
					mDownloadEventListener.onDownloadFailed(thisLoader, "failure");				
				}
			});
		}
	}

	@Override
	public void onResponse(Response arg0) throws IOException {
		
		final URLLoader thisLoader = this;
		final Response response = arg0;
		
		if (mDownloadEventListener != null)
		{
			// Post back to main thread to avoid re-entrancy that breaks OkHTTP.
			PlayerViewController.currentController.post(new Runnable()
			{
				@Override
				public void run() {
					String r;
					try {
						r = response.body().string();
						mDownloadEventListener.onDownloadComplete(thisLoader, r==null?"null" : r);
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
			});
		}		
	}
}
