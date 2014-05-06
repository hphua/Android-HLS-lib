package com.kaltura.hlsplayersdk;

import java.util.Timer;

import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;

import android.content.Context;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.os.Handler;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.widget.MediaController.MediaPlayerControl;
import android.widget.VideoView;

public class PlayerView extends SurfaceView implements VideoPlayerInterface, MediaPlayerControl
{
	// Native Methods
	private native void InitNativeDecoder();
	private native void CloseNativeDecoder();
	private native void PlayFile();
	private native void SetSurface(Surface surface);
	private native void NextFrame();
	
	private int frameDelay = 10;

	
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

	@Override
	public void setVideoUrl(String url) {
		// TODO Auto-generated method stub
		
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
