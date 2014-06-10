package com.kaltura.hlsplayersdk;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.Timer;

import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;
import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestSegment;
import com.kaltura.hlsplayersdk.manifest.events.OnParseCompleteListener;

import android.content.Context;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.os.Handler;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.widget.MediaController.MediaPlayerControl;
import android.widget.VideoView;

public class PlayerView extends SurfaceView implements VideoPlayerInterface, MediaPlayerControl, OnParseCompleteListener, URLLoader.DownloadEventListener 
{
	// Native Methods
	private native void InitNativeDecoder();
	private native void CloseNativeDecoder();
	private native void PlayFile();
	private native void SetSurface(Surface surface);
	private native void NextFrame();
	private native void FeedSegment(String url);
	
	private static PlayerView currentPlayerView = null;
	
	public static void requestNextSegment()
	{
		if (currentPlayerView != null)
		{
			ManifestSegment seg = currentPlayerView.getStreamHandler().getNextFile(0);
			currentPlayerView.FeedSegment(seg.uri);
		}
	}
	
	private int frameDelay = 10;

	// This is our root manifest
	private ManifestParser mManifest = null;
	
	private URLLoader manifestLoader;
	private StreamHandler mStreamHandler = null;
	
	
	private Handler handler = new Handler();
	private Runnable runnable = new Runnable()
	{
		public void run()
		{
			Log.i("Runnable.run", "Running!");
			NextFrame();
			postDelayed(runnable, frameDelay);
		}
	};
	
	// setVideoUrl()
	// Sets the video URL and initiates the download of the manifest
	@Override
	public void setVideoUrl(String url) {
		Log.i("PlayerView.setVideoUrl", url);
		
		manifestLoader = new URLLoader(this, null);
		manifestLoader.get(url);
	}
	
	
	// Called when the manifest parser is complete. Once this is done, play can actually start
	@Override
	public void onParserComplete(ManifestParser parser)
	{
		mStreamHandler = new StreamHandler(parser);
		//mStreamHandler.initialize(parser);
		ManifestSegment seg = getStreamHandler().getFileForTime(0, 0);
		currentPlayerView.FeedSegment(seg.uri);
		play();
		//parser.dumpToLog();
	}
	
	@Override
	public void onDownloadComplete(URLLoader loader, String response)
	{
		mManifest = new ManifestParser();
		mManifest.setOnParseCompleteListener(this);
		mManifest.parse(response, loader.getRequestURI().toString());
	}
	
	@Override
	public void onDownloadFailed(URLLoader loader, String response)
	{
		
	}
	
	// Class Methods
	public PlayerView(Context context)
	{
		
		super(context);
		try
		{
			System.loadLibrary("HLSPlayerSDK");
			InitNativeDecoder();
		}
		catch (Exception e)
		{
			
		}
		currentPlayerView = this;
	}
	
	@Override
	public void close()
	{
		CloseNativeDecoder();
	}
	
	
	@Override
	public String getVideoUrl()
	{
		return "Not Implemented";
	}
	
	public int getDuration()
	{
		return 10;
	}
	
	
	public int getCurrentPosition()
	{
		return 0;
	}
	
	public boolean isPlaying()
	{
		return true;
	}
	
	@Override
	public boolean getIsPlaying()
	{
		return isPlaying();
	}
	
	@Override
	public void play()
	{
		SetSurface(getHolder().getSurface());
		PlayFile();
		this.postDelayed(runnable, frameDelay);
//		if (!this.isPlaying())
//		{
//			super.start();
//		}
	}
	
	public boolean canPause()
	{
		return true;
	}
	
	public boolean canSeekBackward()
	{
		return true;
	}
	
	public boolean canSeekForward()
	{
		return true;		
	}
	
	public int getAudioSessionId()
	{
		return 1;
	}
	
	public int getBufferPercentage()
	{
		return 0;
	}
	
	public void seekTo(int pos)
	{
		
	}
	
	public void start()
	{
		
	}
	
	
	public void pause()
	{
		if (this.isPlaying())
		{
			
			
		}
	}
	
	@Override
	public void stop()
	{
		//super.stopPlayback();
	}
	
	@Override
	public void seek(int msec)
	{
		//super.seekTo(msec);
	}

	public StreamHandler getStreamHandler()
	{
		return mStreamHandler;
	}

	

	@Override
	public void registerPlayerStateChange(OnPlayerStateChangeListener listener) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void registerReadyToPlay(OnPreparedListener listener) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void registerError(OnErrorListener listener) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void registerPlayheadUpdate(OnPlayheadUpdateListener listener) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void registerProgressUpdate(OnProgressListener listener) {
		// TODO Auto-generated method stub
		
	}
	
	
	

}
