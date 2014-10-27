package com.kaltura.playersdk;

import com.kaltura.playersdk.events.OnErrorListener;
import com.kaltura.playersdk.events.OnPlayerStateChangeListener;
import com.kaltura.playersdk.events.OnPlayheadUpdateListener;
import com.kaltura.playersdk.events.OnProgressListener;

public interface VideoPlayerInterface {
    public String getVideoUrl();
    public void setVideoUrl(String url);

    public int getDuration();

    public boolean getIsPlaying();

    public void play();

    public void pause();

    public void stop();

    public void seek(int msec);
    
    public boolean isPlaying();
    
    public void close();

    // events
    public void registerPlayerStateChange(OnPlayerStateChangeListener listener);

    public void registerError(OnErrorListener listener);

    public void registerPlayheadUpdate(OnPlayheadUpdateListener listener);

    public void removePlayheadUpdateListener();
    
    public void registerProgressUpdate(OnProgressListener listener);
    
    /**
     * Set starting point in milliseconds for the next play
     * @param point
     */
    public void setStartingPoint(int point);
    
}
