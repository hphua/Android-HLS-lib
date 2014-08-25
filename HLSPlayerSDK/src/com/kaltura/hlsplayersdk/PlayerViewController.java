package com.kaltura.hlsplayersdk;

import java.util.Vector;

import android.app.Activity;
import android.content.Context;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestSegment;
import com.kaltura.hlsplayersdk.manifest.events.OnParseCompleteListener;
import com.kaltura.hlsplayersdk.subtitles.SubTitleParser;
import com.kaltura.hlsplayersdk.subtitles.SubtitleHandler;
import com.kaltura.hlsplayersdk.subtitles.TextTrackCue;

/**
 * Main class for HLS video playback on the Java side.
 * 
 * PlayerViewController is responsible for integrating the JNI/Native side
 * with the Java APIs and interfaces. This is the central point for HLS
 * video playback!
 */
public class PlayerViewController extends RelativeLayout implements
		VideoPlayerInterface, URLLoader.DownloadEventListener,
		OnParseCompleteListener {

	// State constants.
	private final int STATE_STOPPED = 1;
	private final int STATE_PAUSED = 2;
	private final int STATE_PLAYING = 3;
	private final int STATE_SEEKING = 4;

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
	private native void FeedSegment(String url, int quality, int continuityEra, double startTime);
	private native void SeekTo(double timeInSeconds);
	private native void ApplyFormatChange();

	// Static interface.
	// TODO Allow multiple active PlayerViewController instances.
	private static PlayerViewController currentController = null;
	private static int mQualityLevel = 0;


	/**
	 * Get the next segment in the stream.
	 */
	public static void requestNextSegment() {
		if (currentController == null)
			return;
		
		ManifestSegment seg = currentController.getStreamHandler().getNextFile(mQualityLevel);
		if(seg == null)
			return;

		currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.startTime);
	}

	/**
	 * Initiate loading of the segment corresponding to the specified time.
	 * @param time The time in seconds to request.
	 * @return Offset into the segment to get to exactly the requested time.
	 */
	public static double requestSegmentForTime(double time) {
		if(currentController == null)
			return 0;
		
		ManifestSegment seg = currentController.getStreamHandler().getFileForTime(time, mQualityLevel);
		if(seg == null)
			return 0;
		
		currentController.FeedSegment(seg.uri, seg.quality, seg.continuityEra, seg.startTime);
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

		Log.i("PlayerViewController", "Initializing hw surface.");
		
		if (currentController.mPlayerView != null) {
			currentController.removeView(currentController.mPlayerView);
		}

		LayoutParams lp = new LayoutParams(ViewGroup.LayoutParams.FILL_PARENT,
				ViewGroup.LayoutParams.FILL_PARENT);
		lp.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
		currentController.mPlayerView = new PlayerView(
				currentController.mActivity, currentController,
				enablePushBuffers);
		currentController.addView(currentController.mPlayerView, lp);

		Log.w("addComponents", "Surface Holder is " + currentController.mPlayerView.getHolder());
		if (currentController.mPlayerView.getHolder() != null)
			Log.w("addComponents", "Surface Holder is " + currentController.mPlayerView.getHolder().getSurface());

		// Preserve resolution info for layout.
		setVideoResolution(currentController.mVideoWidth, currentController.mVideoHeight);
	}

	/**
	 * Handle changes in the video resolution. Primarily for correct layout.
	 * @param w Actual width of video.
	 * @param h Actual height of video.
	 */
	public static void setVideoResolution(int w, int h) {
		if (currentController != null) 
		{
			currentController.mVideoWidth = w;
			currentController.mVideoHeight = h;
			
			if(currentController.mPlayerView != null)
			{
				currentController.mPlayerView.mVideoWidth = w;
				currentController.mPlayerView.mVideoHeight = h;
				currentController.mPlayerView.requestLayout();
			}
			
			currentController.requestLayout();
		}
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
				if (state == STATE_PLAYING) {
					int rval = NextFrame();
					if (rval >= 0) mTimeMS = rval;
					if (rval < 0) Log.i("videoThread", "NextFrame() returned " + rval);
					if (rval == -1013) // INFO_DISCONTINUITY
					{
						Log.i("videoThread", "Ran into a discontinuity");
						HandleFormatChange();
					}
					else if (mPlayheadUpdateListener != null)
						mPlayheadUpdateListener.onPlayheadUpdated(mTimeMS);

					// SUBTITLES!
					
					if (mSubtitleHandler != null)
					{
						double time = ( (double)mTimeMS / 1000.0);
						mSubtitleHandler.update(time, 0);
					}
					
					try {
						Thread.yield();
					} catch (Exception e) {
						Log.i("video run", "Video thread sleep interrupted!");
					}

				} else {
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
		CloseNativeDecoder();
	}

	/**
	 * Called when the manifest parser is complete. Once this is done, play can
	 * actually start.
	 */
	public void onParserComplete(ManifestParser parser) {
		Log.i(this.getClass().getName() + ".onParserComplete", "Entered");
		mStreamHandler = new StreamHandler(parser);
		mSubtitleHandler = new SubtitleHandler(parser);
		if (!mSubtitleHandler.hasSubtitles())
		{
			mSubtitleHandler = null;
		}
		
		ManifestSegment seg = getStreamHandler().getFileForTime(0, 0);
		FeedSegment(seg.uri, 0, seg.continuityEra, seg.startTime);

		seg = getStreamHandler().getNextFile(0);
		FeedSegment(seg.uri, 0, seg.continuityEra, seg.startTime);
		
		play();
		
		// Fire prepared event.
		if(mPreparedListener != null)
			mPreparedListener.onPrepared(null);		
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
		if (mStreamHandler != null)
		{
			int ql = mStreamHandler.getQualityLevels();
			if (mQualityLevel < ql -1)
			{
				++mQualityLevel;
			}
		}
	}
	
	public void decrementQuality()
	{
		if (mQualityLevel > 0)
			--mQualityLevel;
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
	    NetworkInfo networkInfo = connMgr.getActiveNetworkInfo();
	    return (networkInfo != null && networkInfo.isConnected());
	}  
	
	public void setVideoUrl(String url) {
		Log.i("PlayerView.setVideoUrl", url);
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
}
