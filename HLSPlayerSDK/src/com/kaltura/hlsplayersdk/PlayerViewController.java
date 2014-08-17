package com.kaltura.hlsplayersdk;

import android.app.Activity;
import android.content.Context;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnPlayheadUpdateListener;
import com.kaltura.hlsplayersdk.events.OnProgressListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestSegment;
import com.kaltura.hlsplayersdk.manifest.events.OnParseCompleteListener;

public class PlayerViewController extends RelativeLayout implements
		VideoPlayerInterface, URLLoader.DownloadEventListener,
		OnParseCompleteListener {
	private PlayerView mPlayerView;
	private Activity mActivity;

	private final int STATE_STOPPED = 1;
	private final int STATE_PAUSED = 2;
	private final int STATE_PLAYING = 3;
	private final int STATE_SEEKING = 4;

	// Native Methods
	private native void InitNativeDecoder();

	private native void CloseNativeDecoder();

	private native void ResetPlayer();

	private native void PlayFile();

	private native void StopPlayer();

	private native void TogglePause();

	public native void SetSurface(Surface surface);

	private native int NextFrame();

	private native void FeedSegment(String url, int quality, double startTime);

	private native void SeekTo(double time); // seconds, not miliseconds - I'll
												// change this later if it

	private native int GetState();

	private static PlayerViewController currentController = null;

	public static void requestNextSegment() {
		if (currentController != null) {
			ManifestSegment seg = currentController.getStreamHandler()
					.getNextFile(0);
			if (seg != null) {
				currentController.FeedSegment(seg.uri, 0, seg.startTime);
			}
		}
	}

	public static double requestSegmentForTime(double time) {
		if (currentController != null) {
			ManifestSegment seg = currentController.getStreamHandler()
					.getFileForTime(time, 0);
			currentController.FeedSegment(seg.uri, 0, seg.startTime);
			return seg.startTime;
		}
		return 0;
	}

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

		Log.w("addComponents", "Surface Holder is "
				+ currentController.mPlayerView.getHolder());
		if (currentController.mPlayerView.getHolder() != null)
			Log.w("addComponents", "Surface Holder is "
					+ currentController.mPlayerView.getHolder().getSurface());
	}

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

	// This is our root manifest
	private ManifestParser mManifest = null;

	private URLLoader manifestLoader;
	private StreamHandler mStreamHandler = null;

	protected StreamHandler getStreamHandler() {
		return mStreamHandler;
	}

	private int mTimeMS = 0;

	public OnPlayheadUpdateListener mPlayheadUpdateListener;

	private Thread mRenderThread;

	private Runnable runnable = new Runnable() {
		public void run() {
			while (true) {
				int state = GetState();
				if (state == STATE_PLAYING) {
					mTimeMS = NextFrame();
					if (mPlayheadUpdateListener != null)
						mPlayheadUpdateListener.onPlayheadUpdated(mTimeMS);
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

	public int mVideoWidth = 640, mVideoHeight = 480;

	// Called when the manifest parser is complete. Once this is done, play can
	// actually start
	public void onParserComplete(ManifestParser parser) {
		Log.i("PlayerView.onParserComplete", "Entered");
		mStreamHandler = new StreamHandler(parser);
		// mStreamHandler.initialize(parser);
		ManifestSegment seg = getStreamHandler().getFileForTime(0, 0);
		currentController.FeedSegment(seg.uri, 0, seg.startTime);
		seg = getStreamHandler().getNextFile(0);
		currentController.FeedSegment(seg.uri, 0, seg.startTime);
		play();
		// parser.dumpToLog();
	}

	@Override
	public void onDownloadComplete(URLLoader loader, String response) {
		mManifest = new ManifestParser();
		mManifest.setOnParseCompleteListener(this);
		mManifest.parse(response, loader.getRequestURI().toString());
	}

	public void onDownloadFailed(URLLoader loader, String response) {

	}

	public PlayerViewController(Context context) {
		super(context);
		initializeNative();
	}

	public PlayerViewController(Context context, AttributeSet attrs) {
		super(context, attrs);
		initializeNative();
	}

	public PlayerViewController(Context context, AttributeSet attrs,
			int defStyle) {
		super(context, attrs, defStyle);
		initializeNative();
	}

	private void initializeNative() {
		try {
			System.loadLibrary("HLSPlayerSDK");
			InitNativeDecoder();
		} catch (Exception e) {
			Log.i("PlayerViewController", "Failed to initialize native video library.");
		}
		
		currentController = this;

		// Kick off render thread.
		mRenderThread = new Thread(runnable, "RenderThread");
		mRenderThread.start();
	}

	public void close() {
		mRenderThread.interrupt();
		CloseNativeDecoder();
	}

	public void setOnFullScreenListener(OnToggleFullScreenListener listener) {

	}

	public boolean getIsPlaying() {
		return GetState() == STATE_PLAYING;
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

		setBackgroundColor(0xFF000000);
	}

	@Override
	protected void onSizeChanged(int w, int h, int oldw, int oldh) {
		super.onSizeChanged(w, h, oldw, oldh);
		Log.i("PlayerViewController.onSizeChanged", "Set size to " + w + "x"
				+ h);
	}

	public void destroy() {
		if (mPlayerView == null)
			return;

		stop();
		close();
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

	public void setVideoUrl(String url) {
		Log.i("PlayerView.setVideoUrl", url);
		// layoutParams lp = this.getLayoutParams();
		stop(); // We don't call StopPlayer here because we want to stop
				// everything, including the update pump
		ResetPlayer();
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
