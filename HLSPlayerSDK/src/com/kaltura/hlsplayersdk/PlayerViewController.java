package com.kaltura.hlsplayersdk;

import java.util.ArrayList;
import java.util.List;
import java.util.Vector;

import android.app.Activity;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
import com.kaltura.hlsplayersdk.cache.SegmentCachedListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestSegment;
import com.kaltura.hlsplayersdk.manifest.events.OnParseCompleteListener;
import com.kaltura.hlsplayersdk.subtitles.SubtitleHandler;
import com.kaltura.hlsplayersdk.subtitles.TextTrackCue;
import com.kaltura.hlsplayersdk.types.PlayerStates;
import com.kaltura.playersdk.AlternateAudioTracksInterface;
import com.kaltura.playersdk.QualityTrack;
import com.kaltura.playersdk.QualityTracksInterface;
import com.kaltura.playersdk.TextTracksInterface;
import com.kaltura.playersdk.VideoPlayerInterface;
import com.kaltura.playersdk.events.OnAudioTrackSwitchingListener;
import com.kaltura.playersdk.events.OnAudioTracksListListener;
import com.kaltura.playersdk.events.OnErrorListener;
import com.kaltura.playersdk.events.OnPlayerStateChangeListener;
import com.kaltura.playersdk.events.OnPlayheadUpdateListener;
import com.kaltura.playersdk.events.OnProgressListener;
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
	
	private final int THREAD_STATE_STOPPED = 0;
	private final int THREAD_STATE_RUNNING = 1;

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
	private native void SetSegmentCountToBuffer(int segmentCount);
	private native int DroppedFramesPerSecond();
	public native boolean AllowAllProfiles();

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
				currentController.SetSurface(null);
				
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
			currentController.postAudioTrackSwitchingEnd(audioTrack);
				
		}
	}
	
	/**
	 *  Provides a method for the native code to notify us that a format change event has occurred
	 */
	public static void notifyFormatChangeComplete(int qualityLevel)
	{
		if (currentController != null)
		{
			currentController.postQualityTrackSwitchingEnd(qualityLevel);
		}
	}
	
	// Interface thread
	static class InterfaceThread extends HandlerThread
	{
		private Handler mHandler = null;
		
		InterfaceThread()
		{
			super("InterfaceThread");
			start();
			setHandler(new Handler(getLooper()));
		}

		public Handler getHandler() {
			return mHandler;
		}

		private void setHandler(Handler mHandler) {
			this.mHandler = mHandler;
		}
		
	}
	
	private InterfaceThread mInterfaceThread = null;
	
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


	private OnPlayheadUpdateListener mPlayheadUpdateListener = null;
	private OnProgressListener mOnProgressListener = null;

	// Video state.
	public int mVideoWidth = 640, mVideoHeight = 480;
	private int mTimeMS = 0;

	// Thread to run video rendering.
	private boolean stopVideoThread = false;
	private int mRenderThreadState = THREAD_STATE_STOPPED;
	private Thread mRenderThread;
	private Runnable renderRunnable = new Runnable() {
		public void run() {
			mRenderThreadState = THREAD_STATE_RUNNING;
			while (mRenderThreadState == THREAD_STATE_RUNNING) {
				if (stopVideoThread)
				{
					Log.i("videoThread", "Stopping video render thread");
					mRenderThreadState = THREAD_STATE_STOPPED;
					continue;
				}
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
					else postPlayheadUpdate(mTimeMS);

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
								postTextTrackText(cue.startTime, cue.endTime - cue.startTime, cue.buffer);
							}
						}
					}
					
					try {
						Thread.yield();
					} catch (Exception e) {
						Log.i("video run", "Video thread sleep interrupted!");
					}
					
					Log.i("PlayerViewController", "Dropped Frames Per Sec: " + DroppedFramesPerSecond());

				} 
				else {
					try {
						Thread.sleep(30);
					} catch (InterruptedException ie) {
						Log.i("video run", "Video thread sleep interrupted!");
					}
				}

			}
			stopVideoThread = false;
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
	}

	public PlayerViewController(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public PlayerViewController(Context context, AttributeSet attrs,
			int defStyle) {
		super(context, attrs, defStyle);
	}

	/**
	 * Load JNI libraries and set up the render thread.
	 */
	private void initializeNative() {
		try {
			System.loadLibrary("HLSPlayerSDK");
			InitNativeDecoder();
			mInterfaceThread = new InterfaceThread();

		} catch (Exception e) {
			Log.i("PlayerViewController", "Failed to initialize native video library.");
		}
		
		// Note the active controller.
		currentController = this;

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
	 *  Reset any state that we have
	 */
	public void reset()
	{
		mTimeMS = 0;
	}

	/**
	 * Called when the manifest parser is complete. Once this is done, play can
	 * actually start.
	 */
	public void onParserComplete(ManifestParser parser) {
		if (parser == null)
		{
			Log.w("PlayerViewController", "Manifest is null. Ending playback.");
			postError(OnErrorListener.MEDIA_ERROR_NOT_VALID, "No Valid Manifest");
			return;
		}
		noMoreSegments = false;
		Log.i(this.getClass().getName() + ".onParserComplete", "Entered");
		mStreamHandler = new StreamHandler(parser);
		mSubtitleHandler = new SubtitleHandler(parser);
		
		ManifestSegment seg = getStreamHandler().getFileForTime(0, 0);
		if (seg == null)
		{
			postError(OnErrorListener.MEDIA_ERROR_NOT_VALID, "Manifest is not valid. There aren't any segments.");
			Log.w("PlayerViewController", "Manifest is not valid. There aren't any segments. Ending playback.");
			return;
		}
		
		int precacheCount = SetSegmentsToBuffer(); // Make sure the segments to buffer count
												   // is set correctly in case it was changed
												   // before starting playback
		
		if (mSubtitleHandler.hasSubtitles())
		{
			postTextTracksList(mSubtitleHandler.getLanguageList(), mSubtitleHandler.getDefaultLanguageIndex());
			
			mSubtitleLanguage = mSubtitleHandler.getDefaultLanguageIndex();
			mSubtitleHandler.precacheSegmentAtTime(0, mSubtitleLanguage );
			postTextTrackChanged(mSubtitleLanguage);
		}
		else
		{
			mSubtitleHandler = null;
			postTextTracksList(new ArrayList<String>(), -1);
			
		}
		
		if (mStreamHandler.hasAltAudio())
		{
			postAudioTracksList(mStreamHandler.getAltAudioLanguageList(), mStreamHandler.getAltAudioDefaultIndex());
		}
		else
		{
			postAudioTracksList(new ArrayList<String>(), -1);
		}
		
		postQualityTracksList(mStreamHandler.getQualityTrackList(), 0);
		

		if (seg.altAudioSegment != null)
		{
			// We need to feed the segment before calling precache so that the datasource can be initialized before we
			// supply the event handler to the segment cache. In the case where the segment is already in the cache, the
			// event handler can be called immediately.
			FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.altAudioSegment.uri, seg.altAudioSegment.altAudioIndex, seg.startTime, seg.cryptoId, seg.altAudioSegment.cryptoId);
			HLSSegmentCache.precache(seg.uri, seg.cryptoId, this, GetInterfaceThread().getHandler());
			postAudioTrackSwitchingStart(-1, seg.altAudioSegment.altAudioIndex);
			postAudioTrackSwitchingEnd(seg.altAudioSegment.altAudioIndex);
		}
		else
		{
			// We need to feed the segment before calling precache so that the datasource can be initialized before we
			// supply the event handler to the segment cache. In the case where the segment is already in the cache, the
			// event handler can be called immediately.
			FeedSegment(seg.uri, seg.quality, seg.continuityEra, null, -1, seg.startTime, seg.cryptoId, -1);
			HLSSegmentCache.precache(seg.uri, seg.cryptoId, this, GetInterfaceThread().getHandler());
		}
		
		// Kick off render thread.
		if (mRenderThreadState == THREAD_STATE_STOPPED);
		{
			mRenderThread = new Thread(renderRunnable, "RenderThread");
			mRenderThread.start();
		}
		
	}
	
	@Override
	public void onSegmentCompleted(String uri) {
		HLSSegmentCache.cancelCacheEvent(uri);
		
		play();	
	}

	@Override
	public void onSegmentFailed(String uri, int responseCode) {

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
		postError(OnErrorListener.MEDIA_ERROR_IO, loader.uri + " (" + response + ")");
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
		return getIsPlaying();
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
		postPlayerStateChange(PlayerStates.PLAY);
	}

	public void pause() {
		GetInterfaceThread().getHandler().post(new Runnable() {
			public void run()
			{
				TogglePause();
				int state = GetState();
				if (state == STATE_PAUSED) postPlayerStateChange(PlayerStates.PAUSE);
				else if (state == STATE_PLAYING) postPlayerStateChange(PlayerStates.PLAY);
			}
		});
	}

	public void stop() {
		HLSSegmentCache.cancelDownloads();
		StopPlayer();
		try {
			Thread.sleep(100);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		postPlayerStateChange(PlayerStates.END);
	}

	public int getCurrentPosition() {
		return mTimeMS;
	}

	private int targetSeekMS = 0;
	private boolean targetSeekSet = false;
	
	public void seek(final int msec) {
		HLSSegmentCache.cancelAllCacheEvents();
		
		targetSeekSet = true;
		targetSeekMS = msec;	
				
		GetInterfaceThread().getHandler().post( new Runnable() {
			public void run()
			{
				boolean tss = targetSeekSet;
				int tsms = targetSeekMS;
				int state = GetState();
				if (tss && state != STATE_STOPPED)
				{
					postPlayerStateChange(PlayerStates.SEEKING);
					targetSeekSet = false;
					targetSeekMS = 0;
					SeekTo(((double)tsms) / 1000.0f);
					postPlayerStateChange(PlayerStates.SEEKED);
				}
				else if (state == STATE_STOPPED)
				{
					Log.i("PlayerViewController.Seek().Runnable()", "Seek halted - player is stopped.");
				}
				else
				{
					Log.i("PlayerViewController.Seek().Runnable()", "No More Seeks Queued");
				}
			}
		});
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
	
	public void stopAndReset()
	{
		if (mRenderThreadState == THREAD_STATE_RUNNING)
			stopVideoThread = true;
		StopPlayer();
		ResetPlayer();
		reset();
		try {
			Thread.yield();
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void setVideoUrl(String url) {
		Log.i("PlayerView.setVideoUrl", url);
		if (manifestLoader != null)
		{
			manifestLoader.setDownloadEventListener(null);
			manifestLoader = null;
		}

		HLSSegmentCache.cancelAllCacheEvents();
		HLSSegmentCache.cancelDownloads();
		targetSeekMS = 0;
		targetSeekSet = false;
		stopAndReset();

		postPlayerStateChange(PlayerStates.START);

		// Confirm network is ready to go.
		if(!isOnline())
		{
			Toast.makeText(getContext(), "Not connnected to network; video may not play.", Toast.LENGTH_LONG).show();
		}
		

		postPlayerStateChange(PlayerStates.LOAD);

		// Init loading.
		manifestLoader = new URLLoader(this, null);
		manifestLoader.get(url);
	}


	private OnPlayerStateChangeListener mPlayerStateChangeListener = null;
	
	@Override
	public void registerPlayerStateChange(OnPlayerStateChangeListener listener) {
		mPlayerStateChangeListener = listener;
	}

	private void postPlayerStateChange(final PlayerStates state)
	{
		if (mPlayerStateChangeListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{

				@Override
				public void run() {
					mPlayerStateChangeListener.onStateChanged(state);					
				}
				
			});
		}
	}
	
	@Override
	public void registerPlayheadUpdate(OnPlayheadUpdateListener listener) {
		mPlayheadUpdateListener = listener;
	}
	
	private void postPlayheadUpdate(final int msec)
	{
		if (mPlayheadUpdateListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mPlayheadUpdateListener.onPlayheadUpdated(msec);					
				}
				
			});
		}
	}

	
	@Override
	public void registerProgressUpdate(OnProgressListener listener) {
		mOnProgressListener = listener;

	}
	public void postProgressUpdate(final int progress)
	{
		if (mOnProgressListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnProgressListener.onProgressUpdate(progress);					
				}
				
			});
		}
	}
	
	OnErrorListener mErrorListener = null;
	@Override
	public void registerError(OnErrorListener listener) {
		mErrorListener = listener;
		
	}
	
	public void postError(final int errorCode, final String errorMessage)
	{
		if (mErrorListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mErrorListener.onError(errorCode, errorMessage);
				}
			});
		}
	}
	
	@Override
	public void removePlayheadUpdateListener() {
		if (mPlayheadUpdateListener != null)
			mPlayheadUpdateListener = null;
		
	}
	
	@Override
	public void setStartingPoint(int point) {
		postError(OnErrorListener.MEDIA_ERROR_UNSUPPORTED, "setStartingPoint");
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
	private void postTextTrackText(final double startTime, final double length, final String buffer)
	{
		if (mSubtitleTextListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mSubtitleTextListener.onSubtitleText(startTime, length, buffer);					
				}
			});
		}
	}
	
	@Override
	public void switchTextTrack(int newIndex) {
		if (mSubtitleHandler != null && newIndex < mSubtitleHandler.getLanguageCount())
		{
			mSubtitleLanguage = newIndex;
			postTextTrackChanged(mSubtitleLanguage);
		}
		
	}

	@Override
	public void registerTextTracksList(OnTextTracksListListener listener) {
		mOnTextTracksListListener = listener;		
	}
	private void postTextTracksList(final List<String> list, final int defaultTrackIndex)
	{
		if (mOnTextTracksListListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnTextTracksListListener.OnTextTracksList(list, defaultTrackIndex);					
				}				
			});
		}
	}
	
	@Override
	public void registerTextTrackChanged(OnTextTrackChangeListener listener) {
		mOnTextTrackChangeListener = listener;
	}
	private void postTextTrackChanged(final int newTrackIndex )
	{
		if (mOnTextTrackChangeListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnTextTrackChangeListener.onOnTextTrackChanged(newTrackIndex);					
				}				
			});
		}
	}
	
	//////////////////////////////////////////////////////////
	// Alternate Audio interface
	//////////////////////////////////////////////////////////

	@Override
	public void hardSwitchAudioTrack(int newAudioIndex) {
		postError(OnErrorListener.MEDIA_ERROR_UNSUPPORTED, "hardSwitchAudioTrack");
		
	}
	
	@Override
	public void softSwitchAudioTrack(int newAudioIndex) {
		if (getStreamHandler() == null) return; // We haven't started anything yet.

		postAudioTrackSwitchingStart( getStreamHandler().getAltAudioCurrentIndex(), newAudioIndex);
		
		boolean success = getStreamHandler().setAltAudioTrack(newAudioIndex); 

		postAudioTrackSwitchingEnd( getStreamHandler().getAltAudioCurrentIndex());
	}
	
	private OnAudioTracksListListener mOnAudioTracksListListener = null;
	@Override
	public void registerAudioTracksList(OnAudioTracksListListener listener) {
		mOnAudioTracksListListener = listener;
	}
	private void postAudioTracksList(final  List<String> list, final int defaultTrackIndex  )
	{
		if (mOnAudioTracksListListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnAudioTracksListListener.OnAudioTracksList(list, defaultTrackIndex);					
				}				
			});
		}
	}
	
	private OnAudioTrackSwitchingListener mOnAudioTrackSwitchingListener = null;
	@Override
	public void registerAudioSwitchingChange( OnAudioTrackSwitchingListener listener) {
		mOnAudioTrackSwitchingListener = listener;
	}
	
	private void postAudioTrackSwitchingStart(final  int oldTrackIndex, final int newTrackIndex  )
	{
		if (mOnAudioTrackSwitchingListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnAudioTrackSwitchingListener.onAudioSwitchingStart(oldTrackIndex, newTrackIndex);					
				}				
			});
		}
	}
	
	private void postAudioTrackSwitchingEnd(final int newTrackIndex  )
	{
		if (mOnAudioTrackSwitchingListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnAudioTrackSwitchingListener.onAudioSwitchingEnd(newTrackIndex);					
				}				
			});
		}
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
				postQualityTrackSwitchingStart(mQualityLevel, newIndex);
				mQualityLevel = newIndex;
			}
			else
			{
				postQualityTrackSwitchingEnd(mQualityLevel);
			}
		}
	}
	@Override
	public void setAutoSwitch(boolean autoSwitch) {
		postError(OnErrorListener.MEDIA_ERROR_UNSUPPORTED, "setAutoSwitch");
		
	}
	
	private OnQualityTracksListListener mOnQualityTracksListListener = null;
	@Override
	public void registerQualityTracksList(OnQualityTracksListListener listener) {
		mOnQualityTracksListListener = listener;
	}
	
	private void postQualityTracksList(final  List<QualityTrack> list, final int defaultTrackIndex  )
	{
		if (mOnQualityTracksListListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnQualityTracksListListener.OnQualityTracksList(list, defaultTrackIndex);					
				}				
			});
		}
	}
	
	
	private OnQualitySwitchingListener mOnQualitySwitchingListener = null;
	@Override
	public void registerQualitySwitchingChange( OnQualitySwitchingListener listener) {
		mOnQualitySwitchingListener = listener;		
	}
	
	private void postQualityTrackSwitchingStart(final  int oldTrackIndex, final int newTrackIndex  )
	{
		if (mOnQualitySwitchingListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnQualitySwitchingListener.onQualitySwitchingStart(oldTrackIndex, newTrackIndex);					
				}				
			});
		}
	}
	
	private void postQualityTrackSwitchingEnd(final int newTrackIndex  )
	{
		if (mOnQualitySwitchingListener != null)
		{
			mActivity.runOnUiThread(new Runnable()
			{
				@Override
				public void run() {
					mOnQualitySwitchingListener.onQualitySwitchingEnd(newTrackIndex);					
				}				
			});
		}
	}
	
	private int mTimeToBuffer = 10;
	private int SetSegmentsToBuffer()
	{
		ManifestParser m = mStreamHandler.getManifestForQuality(mQualityLevel);
		int segments = 1;
		if (m != null)
		{
			segments = mTimeToBuffer / (int)m.targetDuration;
			int rem = mTimeToBuffer % (int)m.targetDuration;
			if (rem != 0) ++segments;
			if (segments > 1) segments = 1;
			SetSegmentCountToBuffer(segments);
			
		}
		else
			SetSegmentCountToBuffer(segments);
		
		return segments;
	}
	
	@Override
	public void setBufferTime(int newTime) {
		mTimeToBuffer = newTime;
		if (mStreamHandler != null && mStreamHandler.manifest != null)
		{
			SetSegmentsToBuffer();
		}
	}
	@Override
	public float getLastDownloadTransferRate() {
		return (float)HLSSegmentCache.lastDownloadDataRate;
	}
	@Override
	public float getDroppedFramesPerSecond() {
		return DroppedFramesPerSecond();
	}
	@Override
	public float getBufferPercentage() {
		return HLSSegmentCache.lastBufferPct;
	}
	@Override
	public int getCurrentQualityIndex() {
		if (mStreamHandler != null) return mStreamHandler.lastQuality;
		return 0;
	}




	
}
