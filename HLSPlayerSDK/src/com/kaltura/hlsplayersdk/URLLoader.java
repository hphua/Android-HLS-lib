package com.kaltura.hlsplayersdk;

import android.util.Log;

import com.kaltura.hlsplayersdk.manifest.BaseManifestItem;
import com.loopj.android.http.*;

public class URLLoader extends AsyncHttpResponseHandler 
{
	private static AsyncHttpClient httpClient = new AsyncHttpClient();
	
	public BaseManifestItem manifestItem = null;
	
	public URLLoader(DownloadEventListener eventListener, BaseManifestItem item)
	{
		Log.i("URLLoader.URLLoader()", "Constructing");
		setDownloadEventListener( eventListener );
		manifestItem = item;
	}
	
	
	public void get(String url)
	{
		httpClient.get(url, this);
	}
	
	
	@Override
	public void onSuccess(String response)
	{
		//Log.i("URLLoader.onSuccess", response);
		if (mDownloadEventListener != null) mDownloadEventListener.onDownloadComplete(this, response);
	}
	
	@Override
	public void onFailure(Throwable error, String content)
	{
		Log.i("URLLoader.onFailure", content==null?"null" : content);
		if(mDownloadEventListener != null) mDownloadEventListener.onDownloadFailed(this, content==null?"null" : content);
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
}
