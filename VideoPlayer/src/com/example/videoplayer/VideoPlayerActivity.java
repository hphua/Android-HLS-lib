package com.example.videoplayer;

import java.util.List;

import android.app.AlertDialog;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v7.app.ActionBarActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.kaltura.hlsplayersdk.HLSPlayerViewController;
import com.kaltura.hlsplayersdk.types.PlayerStates;
import com.kaltura.hlsplayersdk.QualityTrack;
import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
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
	private Runnable soakRunnable = new SoakerRunnable(this);

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
    	Log.i("VideoPlayer UI", " -----> Play " + url);
    	if (playerView != null)
		{
    		clearDebugInfo();
    		playerView.setVideoUrl(url);
    		mLastTimeMS = 0;
    		curQualityLevel = 0;
        	playerView.setVisibility(View.VISIBLE);
        	playerView.play();
		}
    }
    
    boolean menuPrepared = false;
    
    public static final int MENU_START = Menu.FIRST;
    public static final int MENU_AD_TV_GRAB = MENU_START;
    public static final int MENU_OFFICE_SPORTS = MENU_START + 1;
    public static final int MENU_EINAT = MENU_START + 2;
    public static final int MENU_KALTURA_STREAMER = MENU_START + 3;
    public static final int MENU_UPLYNK = MENU_START + 4;
    public static final int MENU_AD_STITCHING = MENU_START + 5;
    public static final int MENU_NASA = MENU_START + 6;
    public static final int MENU_SHORT = MENU_START + 7;
    
    
    @Override
    public boolean onPrepareOptionsMenu(Menu menu)
    {
    	if (!menuPrepared)
    	{
    		MenuItem item = menu.getItem(0);
    		if (item.hasSubMenu())
    		{
    			SubMenu subMenu = item.getSubMenu();
    			subMenu.add(0, MENU_AD_TV_GRAB, Menu.NONE, "AD/TV Grab" );
    			subMenu.add(0, MENU_OFFICE_SPORTS, Menu.NONE, "Office Sports");
    			subMenu.add(0, MENU_EINAT, Menu.NONE, "Einat");
    			subMenu.add(0, MENU_KALTURA_STREAMER, Menu.NONE, "Kaltura Streamer");
    			subMenu.add(0, MENU_UPLYNK, Menu.NONE, "Uplynk");
    			subMenu.add(0, MENU_AD_STITCHING, Menu.NONE, "Ad Stitching");
    			subMenu.add(0, MENU_NASA, Menu.NONE, "Nasa");
    			subMenu.add(0, MENU_SHORT, Menu.NONE, "Short");
    			menuPrepared = true;
    		}
    	}
    	return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        runSoak = false;
        if (id == R.id.kaltura_vod) {
        	setTitle(item.getTitle());
        	lastUrl = "http://www.kaltura.com/p/0/playManifest/entryId/1_0i2t7w0i/format/applehttp";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_AD_TV_GRAB)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://playertest.longtailvideo.com/adaptive/captions/playlist.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_OFFICE_SPORTS)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://www.kaltura.com/p/243342/sp/24334200/playManifest/entryId/1_23pqn2nu/flavorIds/1_vy4eeqwx/format/applehttp/protocol/http/a.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_EINAT)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://apache-testing.dev.kaltura.com/p/1091/sp/109100/playManifest/entryId/0_f8re4ujs/format/applehttp/protocol/http/uiConfId/15088771/a.m3u8?referrer=aHR0cDovL2V4dGVybmFsdGVzdHMuZGV2LmthbHR1cmEuY29t";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_KALTURA_STREAMER)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://apache-testing.dev.kaltura.com/p/1091/sp/109100/playManifest/entryId/0_ue0n7lmw/format/applehttp/protocol/http/uiConfId/15088771/a.m3u8?referrer=aHR0cDovL2V4dGVybmFsdGVzdHMuZGV2LmthbHR1cmEuY29t&responseFormat=m3u8&callback=jQuery111106298717719037086_1425418291289&_=1425418291290";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_UPLYNK)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://content.uplynk.com/2bc2287cdfc4429eb632f7f211eb25b9.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_SHORT)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://apache-testing.dev.kaltura.com/p/1091/sp/109100/playManifest/entryId/0_0fq66zlh/flavorIds/0_3dsewp08,0_hxydxtc8,0_1tqp0z5y/format/applehttp/protocol/http/a.m3u8%3C/url%3E";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_AD_STITCHING)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://lbd.kaltura.com/d2/new.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == MENU_NASA)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://public.infozen.cshls.lldns.net/infozen/public/public.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.abc_dvr_item)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://abclive.abcnews.com/i/abc_live4@136330/master.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.folgers)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://cdnbakmi.kaltura.com/p/243342/sp/24334200/playManifest/entryId/0_uka1msg4/flavorIds/1_vqhfu6uy,1_80sohj7p/format/applehttp/protocol/http/a.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.vod_alt_audio)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://pa-www.kaltura.com/content/shared/erank/multi_audio.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.bipbop)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.aes_vod)
        {
        	setTitle(item.getTitle());
        	lastUrl = "http://live.cdn.antel.net.uy/test/hls/teststream1.m3u8";
            setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.soak)
        {
        	setTitle(item.getTitle());
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

        	// Clock
        	input.setText("http://apache-testing.dev.kaltura.com/p/1091/sp/109100/playManifest/entryId/0_ue0n7lmw/format/applehttp/protocol/http/uiConfId/15088771/a.m3u8?referrer=aHR0cDovL2V4dGVybmFsdGVzdHMuZGV2LmthbHR1cmEuY29t&responseFormat=m3u8&callback=jQuery111106298717719037086_1425418291289&_=1425418291290");

        	// Subtitles
        	input.setText("http://olive.fr.globecast.tv/live/disk4/sub/hls_sub/index.m3u8");

        	playerView.setBufferTime(30);
        	

        	// set up a dialog window
        	alertDialogBuilder
        		.setCancelable(false)
        		.setPositiveButton("OK", new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						// get user input and set it to result
						
						lastUrl = input.getText().toString();
						if (lastUrl.startsWith("http://") || lastUrl.startsWith("https://"))
						{
							setTitle(lastUrl);
							setVideoUrl(lastUrl);
						}
						else
							setTitle("Not a URL: " + lastUrl);
						
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
    String cacheInfo = "";
    
    private void clearDebugInfo()
    {
        subTitleText = "";
        timeStampText = "";
        progressText = "";
        quality = "";
        altAudio = "";
        updateDebugText();
    }
    
    int progressCount = 0;
    
    public void updateDebugText()
    {
    	TextView tv = (TextView)findViewById(R.id.subTitleView);
    	
    	//tv.setBackgroundColor(0x44ffffff);
    	tv.setTextSize(18);
    	tv.setText(HLSPlayerViewController.getVersion() + "\n" + quality + " " + altAudio + " " + timeStampText + HLSSegmentCache.cacheInfo() + "\n" + progressText + subTitleText);
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
		altAudio = "AL:" + (curAltAudioTrack + 1) + "/" + numAltAudioTracks;
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
