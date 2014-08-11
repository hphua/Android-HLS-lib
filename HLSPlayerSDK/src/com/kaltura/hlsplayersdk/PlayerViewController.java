package com.kaltura.hlsplayersdk;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;
import com.kaltura.hlsplayersdk.events.HLSPlayerEventListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.types.PlayerStates;

import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ViewGroup;
import android.view.animation.BounceInterpolator;
import android.widget.RelativeLayout;

public class PlayerViewController extends RelativeLayout implements VideoPlayerInterface
{
	private PlayerView mPlayerView;
	private Activity mActivity;

	
    public PlayerViewController(Context context) {
        super(context);
    }
    
    public PlayerViewController(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public PlayerViewController(Context context, AttributeSet attrs,
            int defStyle) {
        super(context, attrs, defStyle);
    }
    
    public void close()
    {
    	if (mPlayerView != null) mPlayerView.close();
    }
    
    public void setOnFullScreenListener(OnToggleFullScreenListener listener) {
        
    }

    public boolean getIsPlaying()
    {
        if(mPlayerView == null)
            return false;
        return mPlayerView.isPlaying();
    }
    
    /**
     * load given url to the player view
     * 
     * @param iframeUrl
     *            url to payer
     * @param activity
     *            bounding activity
     */
    public void addComponents(String iframeUrl, Activity activity) {
        mActivity = activity;

        LayoutParams lp = new LayoutParams(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.FILL_PARENT);
        mPlayerView = new PlayerView(mActivity);
        addView(mPlayerView, lp);
        
        Log.w("addComponents", "Surface Holder is " + mPlayerView.getHolder());        
        if(mPlayerView.getHolder() != null)
        	Log.w("addComponents", "Surface Holder is " + mPlayerView.getHolder().getSurface());
    }

    @Override
    protected void onSizeChanged (int w, int h, int oldw, int oldh)
    {
        super.onSizeChanged(w, h, oldw, oldh);
        Log.i("PlayerViewController.onSizeChanged", "Set size to " + w + "x" + h);
    }

    public void destroy() {
        if ( mPlayerView!=null )
        {
            mPlayerView.stop();
            mPlayerView.close();
        }
    }
    
    // /////////////////////////////////////////////////////////////////////////////////////////////
    // VideoPlayerInterface methods
    // /////////////////////////////////////////////////////////////////////////////////////////////
    public boolean isPlaying() {
        return (mPlayerView != null && mPlayerView.isPlaying());
    }

    public int getDuration() {
        int duration = 0;
        if (mPlayerView != null)
            duration = mPlayerView.getDuration();

        return duration;
    }

    public String getVideoUrl() {
        String url = null;
        if (mPlayerView != null)
            url = mPlayerView.getVideoUrl();

        return url;
    }

    public void play() {
        if (mPlayerView != null) {
            mPlayerView.play();
        }
    }

    public void pause() {
        if (mPlayerView != null) {
            mPlayerView.pause();
        }
    }

    public void stop() {
        if (mPlayerView != null) {
            mPlayerView.stop();
        }
    }

    public void seek(int msec) {
        if (mPlayerView != null) {
            mPlayerView.seek(mPlayerView.getCurrentPosition() + msec);
        }
    }

    public void setVideoUrl(String url)
    {
    	Log.i("PlayerViewController.setVideoUrl", url);
    	mPlayerView.setVideoUrl(url);
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
