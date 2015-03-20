package com.example.videoplayer;

import java.util.List;
import java.util.Vector;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v7.app.ActionBarActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.kaltura.hlsplayersdk.HLSPlayerViewController;
import com.kaltura.hlsplayersdk.types.PlayerStates;
import com.kaltura.hlsplayersdk.QualityTrack;
import com.kaltura.hlsplayersdk.events.*;

public class VideoPlayerActivity extends ActionBarActivity implements OnTextTracksListListener, OnTextTrackChangeListener,
OnTextTrackTextListener, OnAudioTracksListListener, OnAudioTrackSwitchingListener,
OnQualitySwitchingListener, OnQualityTracksListListener, OnPlayheadUpdateListener, OnPlayerStateChangeListener,
OnProgressListener, OnErrorListener, OnDurationChangedListener  {

	HLSPlayerViewController playerView = null;
	final Context context = this;
	String lastUrl = "";

	int numAltAudioTracks = 0;
	int curAltAudioTrack = -1;

	int numQualityLevels = 0;
	int curQualityLevel = 0;

	int mLastTimeMS = 0;

	boolean runSoak = false;


	private Thread mSoakThread = null;
	private Runnable soakRunnable = new Runnable() {
		public Vector<String> urls = null;
		public void run() {

        	urls = new Vector<String>();
        	urls.add("http://www.kaltura.com/p/0/playManifest/entryId/1_0i2t7w0i/format/applehttp");
        	urls.add("http://abclive.abcnews.com/i/abc_live4@136330/master.m3u8");
        	urls.add("http://cdnbakmi.kaltura.com/p/243342/sp/24334200/playManifest/entryId/0_uka1msg4/flavorIds/1_vqhfu6uy,1_80sohj7p/format/applehttp/protocol/http/a.m3u8");
        	urls.add("http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8");
        	if (playerView.AllowAllProfiles())
        	{
            	urls.add("http://pa-www.kaltura.com/content/shared/erank/multi_audio.m3u8");
        		urls.add("http://live.cdn.antel.net.uy/test/hls/teststream1.m3u8");
        	}

			while (runSoak) {

				runOnUiThread(new Runnable()
					{
						public void run() {
							int i = (int)( Math.random() * urls.size() );
							Log.i("VideoPlayer Soak", "Playing Index (" + i + ") ");

				        	lastUrl = urls.get(i);
				        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
				            setVideoUrl(lastUrl);
						}
					}
				);

				try {
					Thread.sleep((long)(Math.random() * 15000.0) + 5000);
				} catch (InterruptedException ie) {
					Log.i("video run", "Video thread sleep interrupted!");
				}
			}
		}
	};

    @SuppressWarnings("unused")
	@Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_player);

        if(true)
        {
            Log.i("VideoPlayerController", "*** Strict mode enabled.");
            StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
	            .detectDiskReads()
	            .detectDiskWrites()
	            .detectAll()   // or .detectAll() for all detectable problems
	            .penaltyLog()
	            .build());
            StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
	            .detectLeakedSqlLiteObjects()
//	            .detectLeakedClosableObjects()
	            .penaltyLog()
	            .penaltyDeath()
	            .build());
        }

        initPlayerView();

        if(false)
        {
/*            // Test the HLS Segment Cache.
            ByteBuffer googBytes = new ByteBuffer();
            Log.i("HLS Test", "Reading from goog");
            for(int i=0; i<1000; i++)
            {
                long readBytes = HLSSegmentCache.read("https://google.com/?" + Math.random(), 1024, 1024*1024, googBytes);
                try {
        			Log.i("HLS Test", "Got test read " + readBytes + " bytes from goog.com:" + new String(googBytes, "UTF-8"));
        		} catch (UnsupportedEncodingException e) {
        			// TODO Auto-generated catch block
        			e.printStackTrace();
        		}
            } */
        }
    }

    private void initPlayerView()
    {
        try
        {
        	playerView = (HLSPlayerViewController)findViewById(R.id.custom_player);
        	playerView.initialize();
        	playerView.registerTextTracksList(this);
        	playerView.registerTextTrackChanged(this);
        	playerView.registerTextTrackText(this);
        	playerView.registerAudioTracksList(this);
        	playerView.registerAudioSwitchingChange(this);
        	playerView.registerQualityTracksList(this);
        	playerView.registerQualitySwitchingChange(this);
        	playerView.registerPlayheadUpdate(this);
        	playerView.registerPlayerStateChange(this);
        	playerView.registerProgressUpdate(this);
        	playerView.registerError(this);
        	playerView.registerDurationChanged(this);
        }
        catch (Exception e)
        {
        	Log.e("KalturaTestApp", e.getMessage());
        }
    }

    @Override
    public void onStop()
    {
    	if (playerView != null)
    	{
    		playerView.setStartingPoint(mLastTimeMS);
            playerView.release();
            playerView = null;
    	}
    	super.onStop();
    }

    @Override
    public void onRestart()
    {
    	if (playerView == null)
    		initPlayerView();
    	playerView.recoverRelease();
    	super.onRestart();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {

        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.video_player, menu);
        return true;
    }

    @Override
    public boolean onMenuOpened(int featureId, Menu menu)
    {
		return super.onMenuOpened(featureId, menu);
    }

    void setVideoUrl(String url)
    {
    	if (playerView != null)
		{
    		playerView.setVideoUrl(url);
        	playerView.setVisibility(View.VISIBLE);
        	playerView.play();
		}
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        runSoak = false;
        if (id == R.id.kaltura_vod) {
        	lastUrl = "http://www.kaltura.com/p/0/playManifest/entryId/1_0i2t7w0i/format/applehttp";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	//playerView.setVideoUrl("http://kalturavod-vh.akamaihd.net/i/p/1281471/sp/128147100/serveFlavor/entryId/1_0i2t7w0i/v/1/flavorId/1_rncam6d3/index_0_av.m3u8");
        	//playerView.play();
        	return true;
        }
        else if (id == R.id.abc_dvr_item)
        {
        	lastUrl = "http://abclive.abcnews.com/i/abc_live4@136330/master.m3u8";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.folgers)
        {
        	lastUrl = "http://cdnbakmi.kaltura.com/p/243342/sp/24334200/playManifest/entryId/0_uka1msg4/flavorIds/1_vqhfu6uy,1_80sohj7p/format/applehttp/protocol/http/a.m3u8";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.vod_alt_audio)
        {
        	lastUrl = "http://pa-www.kaltura.com/content/shared/erank/multi_audio.m3u8";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.bipbop)
        {
        	lastUrl = "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.aes_vod)
        {
        	lastUrl = "http://live.cdn.antel.net.uy/test/hls/teststream1.m3u8";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.soak)
        {
        	runSoak = true;
        	Log.i("VideoPlayer UI", " -----> Soak");
        	mSoakThread = new Thread(soakRunnable);
        	mSoakThread.start();
        	return true;

        }
        else if (id == R.id.seekFwd)
        {
        	Log.i("VideoPlayer UI", " -----> Seek Fwd");
            playerView.setVisibility(View.VISIBLE);
            playerView.seek(mLastTimeMS + 15000);

        	return true;
        }
        else if (id == R.id.seekBwd)
        {
        	Log.i("VideoPlayer UI", " -----> Seek Bwd");
            playerView.setVisibility(View.VISIBLE);
        	playerView.seek(mLastTimeMS -15000);
        	return true;
        }
        else if (id == R.id.seekFront)
        {
        	Log.i("VideoPlayer UI", " -----> Seek Front");
            playerView.setVisibility(View.VISIBLE);
        	playerView.goToLive();
        	return true;
        }
        else if (id == R.id.testUrl)
        {
        	lastUrl = "http://www.djing.com/tv/live.m3u8";
        	//lastUrl = "http://kapxvideo-a.akamaihd.net/dc-1/m/ny-live-publish21/kLive/smil:0_5zs2oadx_all.smil/playlist.m3u8?DVR";
        	Log.i("VideoPlayer UI", " -----> Play " + lastUrl);
        	setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.pause)
        {
        	Log.i("VideoPlayer UI", " -----> Pause");
        	playerView.pause();
        }
        else if (id == R.id.play)
        {
        	Log.i("VideoPlayer UI", " -----> Play");
        	playerView.play();
        }
        else if (id == R.id.quality_up)
        {
        	Log.i("VideoPlayer UI", " -----> Quality Up");
        	playerView.incrementQuality();
        }
        else if (id == R.id.quality_down)
        {
        	Log.i("VideoPlayer UI", " -----> Quality Down");
        	playerView.decrementQuality();
        }
        else if (id == R.id.audio_up)
        {
        	Log.i("VideoPlayer UI", " -----> Audio Track Up");
        	playerView.hardSwitchAudioTrack(curAltAudioTrack + 1);
        }
        else if (id == R.id.audio_down)
        {
        	Log.i("VideoPlayer UI", " -----> Audio Track Down");
        	playerView.softSwitchAudioTrack(curAltAudioTrack - 1);
        }
        else if (id == R.id.openUrl)
        {
        	// start another popup to enter the URL, somehow
        	LayoutInflater layoutInflater = LayoutInflater.from(context);

        	View urlInputView = layoutInflater.inflate(R.layout.url_input , null);

        	AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);

        	// set url_input.xml to be the layout file of the alertDialog builder
        	alertDialogBuilder.setView(urlInputView );

        	final EditText input = (EditText)urlInputView.findViewById(R.id.userInput);

        	input.setText("http://apache-testing.dev.kaltura.com/p/1091/sp/109100/playManifest/entryId/0_ue0n7lmw/format/applehttp/protocol/http/uiConfId/15088771/a.m3u8?referrer=aHR0cDovL2V4dGVybmFsdGVzdHMuZGV2LmthbHR1cmEuY29t");

        	input.setText("http://cdnapi.kaltura.com/p/1878761/sp/187876100/playManifest/entryId/0_8s3afdlb/format/applehttp/protocol/http/uiConfId/28013271/a.m3u8%22");

        	// Disney Alt Audio
        	input.setText("http://54.169.47.74/masterstitch.m3u8?entryId=126ae943&url=http%3A%2F%2F111.223.97.115%2Fhls%2Fnl%2Findex-timeshifting.m3u8&uid=1c2ef485_46.20.235.45");

        	// Subtitles
        	input.setText("http://olive.fr.globecast.tv/live/disk4/sub/hls_sub/index.m3u8");

        	// set up a dialog window
        	alertDialogBuilder
        		.setCancelable(false)
        		.setPositiveButton("OK", new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						// get user input and set it to result
						lastUrl = input.getText().toString();
						Log.i("VideoPlayer UI", " -----> Play (from input dialog) " + lastUrl);
						setVideoUrl(lastUrl);
					}
				})
				.setNegativeButton("Cancel",
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog,	int id) {
								dialog.cancel();
							}
						});

			// create an alert dialog
			AlertDialog alertD = alertDialogBuilder.create();

			alertD.show();


        }
        return super.onOptionsItemSelected(item);
    }
    
    String subTitleText = "";
    String timeStampText = "";
    String progressText = "";
    String quality = "";
    String altAudio = "";
    
    int progressCount = 0;
    
    public void updateDebugText()
    {
    	TextView tv = (TextView)findViewById(R.id.subTitleView);
    	tv.setText(quality + " " + altAudio + " " + timeStampText + progressText + subTitleText);
    }

	@Override
	public void onSubtitleText(double startTime, double length, String align, String buffer) {
		Log.i("VideoPlayer.onSubtitleText", "Start: " + startTime + " | Length: " + length + " | " + buffer);
		
		subTitleText = "T:" + playerView.getCurrentPosition() + " S:" + startTime + " L:" + length + " A:" + align + "\n" + buffer;
		updateDebugText();

	}
	
	

	@Override
	public void OnTextTracksList(List<String> list, int defaultTrackIndex) {
		Log.i("VideoPlayer.OnTextTracksList", "Count = " + list.size());
		Log.i("VideoPlayer.OnTextTracksList", "Default = " + defaultTrackIndex);
		for (int i = 0; i < list.size(); ++i)
			Log.i("VideoPlayer.OnTextTracksList", "Language[" + i + "] = " + list.get(i));


	}

	@Override
	public void onOnTextTrackChanged(int newTrackIndex) {
		Log.i("VideoPlayer.onOnTextTrackChanged","newTrackIndex = " + newTrackIndex);

	}

	@Override
	public void onAudioSwitchingStart(int oldTrackIndex, int newTrackIndex) {
		Log.i("VideoPlayer.onAudioSwitchingStart", "Quaity Changing from  " + oldTrackIndex + " to " + newTrackIndex);
	}

	@Override
	public void onAudioSwitchingEnd(int newTrackIndex) {
		curAltAudioTrack = newTrackIndex;
		altAudio = "AL:" + curAltAudioTrack + "/" + numAltAudioTracks;
		updateDebugText();
		Log.i("VideoPlayer.onAudioSwitchingEnd", "Language Changed to " + newTrackIndex);

	}

	@Override
	public void OnAudioTracksList(List<String> list, int defaultTrackIndex) {
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Count = " + list.size());
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Default = " + defaultTrackIndex);
		for (int i = 0; i < list.size(); ++i)
			Log.i("VideoPlayer.onAlternateAudioAvailable", "Language[" + i + "] = " + list.get(i));

		numAltAudioTracks = list.size();
	}

	private void LogQualityTrack(QualityTrack track)
	{
		Log.i("QualityTrack", track.trackId + "|" + track.bitrate + "|" + track.height + "|" + track.width + "|" + track.type.toString() );
	}
	
	int numQualityTracks = 0;

	@Override
	public void OnQualityTracksList(List<QualityTrack> list, int defaultTrackIndex) {
		numQualityTracks = list.size();
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Count = " + list.size());
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Default = " + defaultTrackIndex);
		for (int i = 0; i < list.size(); ++i)
			LogQualityTrack(list.get(i));
		curQualityLevel = defaultTrackIndex;
		quality = "Q:" + (curQualityLevel + 1) + "/" + numQualityTracks;
	}

	@Override
	public void onQualitySwitchingStart(int oldTrackIndex, int newTrackIndex) {
		Log.i("VideoPlayer.onQualitySwitchingStart", "Quaity Changing from  " + oldTrackIndex + " to " + newTrackIndex);

	}

	@Override
	public void onQualitySwitchingEnd(int newTrackIndex) {
		curQualityLevel = newTrackIndex;
		Log.i("VideoPlayer.onQualitySwitchingEnd", "Quaity Changed to " + newTrackIndex);
		quality = "Q:" + (curQualityLevel + 1) + "/" + numQualityTracks;
		

	}


	@Override
	public void onPlayheadUpdated(int msec) {
		mLastTimeMS = msec;
		timeStampText = "Time: " + msec + "\n";
		//Log.i("OnPlayheadUpdated", "Time = " + msec);
	
		updateDebugText();
	}

	@Override
	public boolean onStateChanged(PlayerStates state) {
		Log.i("VideoPlayer.onStateChanged", "Player State changed to " + state.toString());


		if (state == PlayerStates.END)
			playerView.setVisibility(View.INVISIBLE);

		return true;
	}

	@Override
	public void onProgressUpdate(int progress) {
		Log.i("VideoPlayer.OnProgressUpdate", "Download Progress: " + progress);
		++progressCount;
		progressText = "Progress: " + progress + "(" + progressCount + ")" + "%\n";
		updateDebugText();


	}

	@Override
	public void onError(int errorCode, String errorMessage)
	{
		Toast.makeText(context, errorMessage + "(" + errorCode + ")", Toast.LENGTH_LONG).show();

	}

	@Override
	public void onFatalError(int errorCode, String errorMessage)
	{
		Toast.makeText(context, "FATAL: " + errorMessage + "(" + errorCode + ")", Toast.LENGTH_LONG).show();

	}

	@Override
	public void onDurationChanged(int msec)
	{
		Log.i("VideoPlayerActivity.onDurationChanged", "Duration = " + msec);
	}
}
