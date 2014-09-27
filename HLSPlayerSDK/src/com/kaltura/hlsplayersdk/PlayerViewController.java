package com.kaltura.hlsplayersdk;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Vector;

import android.app.Activity;
import android.content.Context;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
import com.kaltura.hlsplayersdk.cache.SegmentCachedListener;
import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestSegment;
import com.kaltura.hlsplayersdk.manifest.events.OnParseCompleteListener;
import com.kaltura.hlsplayersdk.subtitles.SubtitleHandler;
import com.kaltura.hlsplayersdk.subtitles.TextTrackCue;
import com.kaltura.playersdk.AlternateAudioTracksInterface;
import com.kaltura.playersdk.QualityTracksInterface;
import com.kaltura.playersdk.TextTracksInterface;
import com.kaltura.playersdk.events.OnAudioTrackSwitchingListener;
import com.kaltura.playersdk.events.OnAudioTracksListListener;
import com.kaltura.playersdk.events.OnQualitySwitchingListener;
import com.kaltura.playersdk.events.OnQualityTracksListListener;
import com.kaltura.playersdk.events.OnTextTrackChangeListener;
import com.kaltura.playersdk.events.OnTextTrackTextListener;
import com.kaltura.playersdk.events.OnTextTracksListListener;

/**
 * Main class for HLS video playback on the Java side.
 * 
 * PlayerViewController is responsible for integrating the JNI/Native side
 * with the Java APIs and interfaces. This is the central point for HLS
 * video playback!
 */
public class PlayerViewController extends RelativeLayout implements
		VideoPlayerInterface, URLLoader.DownloadEventListener, OnParseCompleteListener, 
		TextTracksInterface, AlternateAudioTracksInterface, QualityTracksInterface, SegmentCachedListener {

	// State constants.
	private final int STATE_STOPPED = 1;
	private final int STATE_PAUSED = 2;
	private final int STATE_PLAYING = 3;
	private final int STATE_SEEKING = 4;
	private final int STATE_FOUND_DISCONTINUITY = 6;

	// Native methods
	private native int GetState();
	private native void InitNativeDecoder();
	private native void CloseNativeDecoder();
	private native void ResetPlayer();
	private native void PlayFile();
	private native void StopPlayer();
	private native void TogglePause();
	public native void SetSurface(Surface surface);
	private native int NextFrame();
	private native void FeedSegment(String url, int quality, int continuityEra, String altAudioURL, int altAudioIndex, double startTime, int cryptoId, int altCryptoId);
	private native void SeekTo(double timeInSeconds);
	private native void ApplyFormatChange();

	// Static interface.
	// TODO Allow multiple active PlayerViewController instances.
	public static PlayerViewController currentController = null;
	private static int mQualityLevel = 0;
	private static int mSubtitleLanguage = 0;
	private static int mAltAudioLanguage = 0;
	
	private static boolean noMoreSegments = false;


	/**
	 * Get the next segment in the stream.
	 */
	public static void requestNextSegment() {
		if (currentController == null)
			return;
		
		ManifestSegment seg = currentController.getStreamHandler().getNextFile(mQualityLevel);
		if(seg == null)
		{
			noMoreSegments = true;
			return;
		}
			

		if (seg.altAudioSegment != null)
		{
			currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.altAudioSegment.uri, seg.altAudioSegment.altAudioIndex, seg.startTime, seg.cryptoId, seg.altAudioSegment.cryptoId);
		}
		else
		{
			currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, null, -1, seg.startTime, seg.cryptoId, -1);
		}
	}

	/**
	 * Initiate loading of the segment corresponding to the specified time.
	 * @param time The time in seconds to request.
	 * @return Offset into the segment to get to exactly the requested time.
	 */
	public static double requestSegmentForTime(double time) {
		Log.i("PlayerViewController.requestSegmentForTime", "Requested Segment Time: " + time);
		if(currentController == null)
			return 0;
		
		ManifestSegment seg = currentController.getStreamHandler().getFileForTime(time, mQualityLevel);
		if(seg == null)
			return 0;
		
		if (seg.altAudioSegment != null)
		{
			currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.altAudioSegment.uri, seg.altAudioSegment.altAudioIndex, seg.startTime, seg.cryptoId, seg.altAudioSegment.cryptoId);
		}
		else
		{
			currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, null, -1, seg.startTime, seg.cryptoId, -1);
		}


		return seg.startTime;
	}

	/**
	 * Internal helper. Creates a SurfaceView with proper parameters for display.
	 * This is needed for compatibility with older devices. When the surface is
	 * ready, SetSurface() is called back from the SurfaceView.
	 * 
	 * @param enablePushBuffers Use the PUSH_BUFFERS surface type?
	 * @param w Desired surface width.
	 * @param h Desired surface height.
	 * @param colf Desired color format.
	 */
	public static void enableHWRendererMode(boolean enablePushBuffers, int w,
			int h, int colf) {

		final boolean epb = enablePushBuffers;

		
		Log.i("PlayerViewController", "Initializing hw surface.");
		
		currentController.mActivity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				
				if (currentController.mPlayerView != null) {
					currentController.removeView(currentController.mPlayerView);
				}
		
				@SuppressWarnings("deprecation")
				LayoutParams lp = new LayoutParams(ViewGroup.LayoutParams.FILL_PARENT,
						ViewGroup.LayoutParams.FILL_PARENT);
				lp.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
				currentController.mPlayerView = new PlayerView(
						currentController.mActivity, currentController,
						epb);
				currentController.addView(currentController.mPlayerView, lp);
		
				Log.w("addComponents", "Surface Holder is " + currentController.mPlayerView.getHolder());
				if (currentController.mPlayerView.getHolder() != null)
					Log.w("addComponents", "Surface Holder is " + currentController.mPlayerView.getHolder().getSurface());
		
				// Preserve resolution info for layout.
				setVideoResolution(currentController.mVideoWidth, currentController.mVideoHeight);

			}
		});
		
	}

	/**
	 * Handle changes in the video resolution. Primarily for correct layout.
	 * @param w Actual width of video.
	 * @param h Actual height of video.
	 */
	public static void setVideoResolution(int w, int h) {
		final int ww = w;
		final int hh = h;
		if (currentController != null) 
		{
			currentController.mActivity.runOnUiThread(new Runnable() {
				@Override
				public void run() {
					currentController.mVideoWidth = ww;
					currentController.mVideoHeight = hh;
					
					if(currentController.mPlayerView != null)
					{
						currentController.mPlayerView.mVideoWidth = ww;
						currentController.mPlayerView.mVideoHeight = hh;
						currentController.mPlayerView.requestLayout();
		
					}
			
					currentController.requestLayout();
				}
			});
		}
	}
	
	/**
	 *  Provides a method for the native code to notify us that a format change event has occurred
	 */
	public static void notifyAudioTrackChangeComplete(int audioTrack)
	{
		if (currentController != null)
		{
			if (currentController.mOnAudioTrackSwitchingListener != null)
			{
				currentController.mOnAudioTrackSwitchingListener.onAudioSwitchingEnd(audioTrack);
			}
				
		}
	}
	
	/**
	 *  Provides a method for the native code to notify us that a format change event has occurred
	 */
	public static void notifyFormatChangeComplete(int qualityLevel)
	{
		if (currentController != null)
		{
			if (currentController.mOnQualitySwitchingListener != null)
			{
				currentController.mOnQualitySwitchingListener.onQualitySwitchingEnd(qualityLevel);
			}
				
		}
	}
	
	// Interface thread
	class InterfaceThread extends Thread
	{
		public Handler mHandler;
		
		public void run()
		{
			Looper.prepare();
			
			mHandler = new Handler()
			{
				public void handleMessage(Message msg)
				{
					
				}
			};
			
			Looper.loop();
		}
	}
	
	private InterfaceThread mInterfaceThread = new InterfaceThread();
	
	public static InterfaceThread GetInterfaceThread()
	{
		return currentController.mInterfaceThread;
	}

	// Instance members.
	private Activity mActivity;
	private PlayerView mPlayerView;

	// This is our root manifest
	private ManifestParser mManifest = null;
	private URLLoader manifestLoader;
	private StreamHandler mStreamHandler = null;
	private SubtitleHandler mSubtitleHandler = null;


	public OnPlayheadUpdateListener mPlayheadUpdateListener;
	public OnPreparedListener mPreparedListener;

	// Video state.
	public int mVideoWidth = 640, mVideoHeight = 480;
	private int mTimeMS = 0;

	// Thread to run video rendering.
	private Thread mRenderThread;
	private Runnable runnable = new Runnable() {
		public void run() {
			while (true) {
				int state = GetState();
				if (state == STATE_PLAYING || state == STATE_FOUND_DISCONTINUITY) {
					int rval = NextFrame();
					if (rval >= 0) mTimeMS = rval;
					if (rval < 0) Log.i("videoThread", "NextFrame() returned " + rval);
					if (rval == -1 && noMoreSegments) currentController.stop();
					if (rval == -1013) // INFO_DISCONTINUITY
					{
						Log.i("videoThread", "Ran into a discontinuity (INFO_DISCONTINUITY)");
						HandleFormatChange();
					}
					else if (mPlayheadUpdateListener != null)
						mPlayheadUpdateListener.onPlayheadUpdated(mTimeMS);

					// SUBTITLES!
					
					if (mSubtitleHandler != null)
					{
						double time = ( (double)mTimeMS / 1000.0);
						Vector<TextTrackCue> cues = mSubtitleHandler.update(time, mSubtitleLanguage);
						if (cues != null && mSubtitleTextListener != null)
						{
							for (int i = 0; i < cues.size(); ++i)
							{
								TextTrackCue cue = cues.get(i);
								mSubtitleTextListener.onSubtitleText(cue.startTime, cue.endTime - cue.startTime, cue.buffer);
							}
						}
					}
					
					try {
						Thread.yield();
					} catch (Exception e) {
						Log.i("video run", "Video thread sleep interrupted!");
					}

				} 
				else {
					try {
						Thread.sleep(30);
					} catch (InterruptedException ie) {
						Log.i("video run", "Video thread sleep interrupted!");
					}
				}

			}
		}
	};
	
	// Handle discontinuity/format change
	public void HandleFormatChange()
	{
		mActivity.runOnUiThread(new Runnable()
			{
				public void run() {
					Log.i("HandleFormatChange", "UI Thread calling ApplyFormatChange()");
					ApplyFormatChange();
				}
			}
		);
	}

	public PlayerViewController(Context context) {
		super(context);
		mInterfaceThread.start();
	}

	public PlayerViewController(Context context, AttributeSet attrs) {
		super(context, attrs);
		mInterfaceThread.start();
	}

	public PlayerViewController(Context context, AttributeSet attrs,
			int defStyle) {
		super(context, attrs, defStyle);
		mInterfaceThread.start();
	}

	/**
	 * Load JNI libraries and set up the render thread.
	 */
	private void initializeNative() {
		try {
			System.loadLibrary("HLSPlayerSDK");
			InitNativeDecoder();
		} catch (Exception e) {
			Log.i("PlayerViewController", "Failed to initialize native video library.");
		}
		
		// Note the active controller.
		currentController = this;

		// Kick off render thread.
		mRenderThread = new Thread(runnable, "RenderThread");
		mRenderThread.start();
	}

	/**
	 * Terminate render thread and shut down JNI resources.
	 */
	public void close() {
		Log.i("PlayerViewController", "Closing resources.");
		mRenderThread.interrupt();
		mInterfaceThread.interrupt();
		CloseNativeDecoder();
		
	}

	/**
	 * Called when the manifest parser is complete. Once this is done, play can
	 * actually start.
	 */
	public void onParserComplete(ManifestParser parser) {
		noMoreSegments = false;
		Log.i(this.getClass().getName() + ".onParserComplete", "Entered");
		mStreamHandler = new StreamHandler(parser);
		mSubtitleHandler = new SubtitleHandler(parser);
		if (mSubtitleHandler.hasSubtitles())
		{
			if (mOnTextTracksListListener != null)
				mOnTextTracksListListener.OnTextTracksList(mSubtitleHandler.getLanguageList(), mSubtitleHandler.getDefaultLanguageIndex());
			
			mSubtitleLanguage = mSubtitleHandler.getDefaultLanguageIndex();
			mSubtitleHandler.precacheSegmentAtTime(0, mSubtitleLanguage );
			if (mOnTextTrackChangeListener != null)
				mOnTextTrackChangeListener.onOnTextTrackChanged(mSubtitleLanguage);
			
					
		}
		else
		{
			mSubtitleHandler = null;
			if (mOnTextTracksListListener != null)
				mOnTextTracksListListener.OnTextTracksList(new ArrayList<String>(), -1);
			
		}
		
		if (mStreamHandler.hasAltAudio())
		{
			if (mOnAudioTracksListListener != null)
				mOnAudioTracksListListener.OnAudioTracksList(mStreamHandler.getAltAudioLanguageList(), mStreamHandler.getAltAudioDefaultIndex());
		}
		else
		{
			if (mOnAudioTracksListListener != null)
				mOnAudioTracksListListener.OnAudioTracksList(new ArrayList<String>(), -1);
		}
		
		if (mOnQualityTracksListListener != null)
		{
			mOnQualityTracksListListener.OnQualityTracksList(mStreamHandler.getQualityTrackList(), 0);
		}
		
		
		ManifestSegment seg = getStreamHandler().getFileForTime(0, 0);
		if (seg.altAudioSegment != null)
		{
			HLSSegmentCache.precache(seg.uri, seg.cryptoId, this);
			FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.altAudioSegment.uri, seg.altAudioSegment.altAudioIndex, seg.startTime, seg.cryptoId, seg.altAudioSegment.cryptoId);
			if (mOnAudioTrackSwitchingListener != null)
			{
				mOnAudioTrackSwitchingListener.onAudioSwitchingStart(-1, seg.altAudioSegment.altAudioIndex);
				mOnAudioTrackSwitchingListener.onAudioSwitchingEnd(seg.altAudioSegment.altAudioIndex);
			}
		}
		else
		{
			HLSSegmentCache.precache(seg.uri, seg.cryptoId, this);
			FeedSegment(seg.uri, seg.quality, seg.continuityEra, null, -1, seg.startTime, seg.cryptoId, -1);
		}


	}
	
	@Override
	public void onSegmentCompleted(String uri) {
		HLSSegmentCache.cancelCacheEvent(uri);
		
		play();
		
		// Fire prepared event.
		if(mPreparedListener != null)
			mPreparedListener.onPrepared(null);		
		
	}
	@Override
	public void onSegmentFailed(String uri, IOException e) {

		HLSSegmentCache.cancelCacheEvent(uri);
		
	}

	@Override
	public void onDownloadComplete(URLLoader loader, String response) {
		mManifest = new ManifestParser();
		mManifest.setOnParseCompleteListener(this);
		mManifest.parse(response, loader.getRequestURI().toString());
	}

	public void onDownloadFailed(URLLoader loader, String response) {
		Log.i("PlayerViewController", "Download failed: " + response);
	}

	protected StreamHandler getStreamHandler() {
		return mStreamHandler;
	}

	public void setOnFullScreenListener(OnToggleFullScreenListener listener) {

	}

	public boolean getIsPlaying() {
		return GetState() == STATE_PLAYING;
	}

	public void addComponents(String iframeUrl, Activity activity) {
		mActivity = activity;
		setBackgroundColor(0xFF000000);
		initializeNative();
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		Log.i("PlayerViewController.onSizeChanged", "Set size to " + w + "x" + h);
	}

	public void destroy() 
	{
		Log.i("PlayerViewController", "Destroying...");

		if (mPlayerView == null)
			return;

		stop();
		close();
	}

	public void incrementQuality()
	{
		switchQualityTrack(mQualityLevel + 1); 
	}
	
	public void decrementQuality()
	{
		switchQualityTrack(mQualityLevel - 1); 
	}

	// /////////////////////////////////////////////////////////////////////////////////////////////
	// VideoPlayerInterface methods
	// /////////////////////////////////////////////////////////////////////////////////////////////
	public boolean isPlaying() {
		return isPlaying();
	}

	public int getDuration() {
		if (mStreamHandler != null)
			return mStreamHandler.getDuration();
		return -1;
	}

	public String getVideoUrl() {
		return "Not Implemented";
	}

	public void play() {
		PlayFile();
	}

	public void pause() {
		TogglePause();
	}

	public void stop() {
		StopPlayer();
		try {
			Thread.sleep(100);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	public int getCurrentPosition() {
		return mTimeMS;
	}

	public void seek(int msec) {
		int curPos = getCurrentPosition();
		SeekTo((curPos + msec) / 1000);
	}

	// Helper to check network status.
	public boolean isOnline() {
	    ConnectivityManager connMgr = (ConnectivityManager) 
	            getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
	    NetworkInfo networkInfo = null;
	    try
	    {
	    	networkInfo = connMgr.getActiveNetworkInfo();
	    }
	    catch (Exception e)
	    {
	    	Log.i("PlayerViewController.isOnline()", e.toString());
	    	Log.i("PlayerViewController.isOnline()", "This is possibly because the permission 'android.permission.ACCESS_NETWORK_STATE' is missing from the manifest.");
	    }
	    return (networkInfo != null && networkInfo.isConnected());
	}  
	
	public void setVideoUrl(String url) {
		Log.i("PlayerView.setVideoUrl", url);
		HLSSegmentCache.cancelAllCacheEvents();
		StopPlayer();
		ResetPlayer();

		// Confirm network is ready to go.
		if(!isOnline())
		{
			Toast.makeText(getContext(), "Not connnected to network; video may not play.", Toast.LENGTH_LONG).show();
		}

		// Init loading.
		manifestLoader = new URLLoader(this, null);
		manifestLoader.get(url);
	}

	@Override
	public void registerPlayerStateChange(OnPlayerStateChangeListener listener) {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerReadyToPlay(OnPreparedListener listener) {
		// TODO Auto-generated method stub
		mPreparedListener = listener;
	}

	@Override
	public void registerError(OnErrorListener listener) {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerPlayheadUpdate(OnPlayheadUpdateListener listener) {
		mPlayheadUpdateListener = listener;
	}

	@Override
	public void registerProgressUpdate(OnProgressListener listener) {
		// TODO Auto-generated method stub

	}
	
	//////////////////////////////////////////////////////////
	// Subtitle interface
	//////////////////////////////////////////////////////////
	private OnTextTracksListListener mOnTextTracksListListener = null;
	private OnTextTrackChangeListener mOnTextTrackChangeListener = null;
	
	private OnTextTrackTextListener mSubtitleTextListener = null;
	public void registerTextTrackText(OnTextTrackTextListener listener)
	{
		mSubtitleTextListener = listener;
	}
	
	@Override
	public void switchTextTrack(int newIndex) {
		if (mSubtitleHandler != null && newIndex < mSubtitleHandler.getLanguageCount())
		{
			mSubtitleLanguage = newIndex;
			if (mOnTextTrackChangeListener != null)
				mOnTextTrackChangeListener.onOnTextTrackChanged(mSubtitleLanguage);
		}
		
	}
	@Override
	public void registerTextTracksList(OnTextTracksListListener listener) {
		mOnTextTracksListListener = listener;		
	}
	@Override
	public void registerTextTrackChanged(OnTextTrackChangeListener listener) {
		mOnTextTrackChangeListener = listener;
		
	}
	
	//////////////////////////////////////////////////////////
	// Alternate Audio interface
	//////////////////////////////////////////////////////////

	@Override
	public void hardSwitchAudioTrack(int newAudioIndex) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void softSwitchAudioTrack(int newAudioIndex) {
		
		if (mOnAudioTrackSwitchingListener != null)
			mOnAudioTrackSwitchingListener.onAudioSwitchingStart( getStreamHandler().getAltAudioCurrentIndex(), newAudioIndex);
		
		boolean success = getStreamHandler().setAltAudioTrack(newAudioIndex); 

		if (!success && mOnAudioTrackSwitchingListener != null)
			mOnAudioTrackSwitchingListener.onAudioSwitchingEnd( getStreamHandler().getAltAudioCurrentIndex());
	}
	
	private OnAudioTracksListListener mOnAudioTracksListListener = null;
	@Override
	public void registerAudioTracksList(OnAudioTracksListListener listener) {
		mOnAudioTracksListListener = listener;
	}
	
	private OnAudioTrackSwitchingListener mOnAudioTrackSwitchingListener = null;
	@Override
	public void registerAudioSwitchingChange( OnAudioTrackSwitchingListener listener) {
		mOnAudioTrackSwitchingListener = listener;
		
	}

	//////////////////////////////////////////////////////////
	// Quality Change interface
	//////////////////////////////////////////////////////////

	@Override
	public void switchQualityTrack(int newIndex) {
		if (mStreamHandler != null)
		{
			int ql = mStreamHandler.getQualityLevels();
			if (newIndex >= 0 && newIndex < ql -1)
			{
				if (mOnQualitySwitchingListener != null)
					mOnQualitySwitchingListener.onQualitySwitchingStart(mQualityLevel, newIndex);
				mQualityLevel = newIndex;
			}
			else
			{
				if (mOnQualitySwitchingListener != null)
					mOnQualitySwitchingListener.onQualitySwitchingEnd(mQualityLevel);
			}
		}
	}
	@Override
	public void setAutoSwitch(boolean autoSwitch) {
		// TODO Auto-generated method stub
		
	}
	
	private OnQualityTracksListListener mOnQualityTracksListListener = null;
	@Override
	public void registerQualityTracksList(OnQualityTracksListListener listener) {
		mOnQualityTracksListListener = listener;
	}
	
	private OnQualitySwitchingListener mOnQualitySwitchingListener = null;
	@Override
	public void registerQualitySwitchingChange( OnQualitySwitchingListener listener) {
		mOnQualitySwitchingListener = listener;		
	}



	
}
