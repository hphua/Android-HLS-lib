package com.example.videoplayer;

import java.util.List;

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

import com.kaltura.hlsplayersdk.PlayerViewController;
import com.kaltura.playersdk.QualityTrack;
import com.kaltura.playersdk.events.*;

public class VideoPlayerActivity extends ActionBarActivity implements OnTextTracksListListener, OnTextTrackChangeListener, 
OnTextTrackTextListener, OnAudioTracksListListener, OnAudioTrackSwitchingListener, 
OnQualitySwitchingListener, OnQualityTracksListListener  {

	PlayerViewController playerView = null;
	final Context context = this;
	String lastUrl = "";
	
	int numAltAudioTracks = 0;
	int curAltAudioTrack = -1;
	
	int numQualityLevels = 0;
	int curQualityLevel = 0;

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
        	playerView = (PlayerViewController)findViewById(R.id.custom_player);
        	playerView.addComponents("", this);
        	playerView.registerTextTracksList(this);
        	playerView.registerTextTrackChanged(this);
        	playerView.registerTextTrackText(this);
        	playerView.registerAudioTracksList(this);
        	playerView.registerAudioSwitchingChange(this);
        	playerView.registerQualityTracksList(this);
        	playerView.registerQualitySwitchingChange(this);
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
            playerView.close();
            playerView = null;
    	}
    	super.onStop();
    }
    
    @Override
    public void onRestart()
    {
    	if (playerView == null)
    		initPlayerView();
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

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        if (id == R.id.kaltura_vod) {
        	lastUrl = "http://www.kaltura.com/p/0/playManifest/entryId/1_0i2t7w0i/format/applehttp";
        	playerView.setVideoUrl(lastUrl);
        	//playerView.setVideoUrl("http://kalturavod-vh.akamaihd.net/i/p/1281471/sp/128147100/serveFlavor/entryId/1_0i2t7w0i/v/1/flavorId/1_rncam6d3/index_0_av.m3u8");
        	//playerView.play();
            return true;
        }
        else if (id == R.id.abc_dvr_item)
        {
        	lastUrl = "http://abclive.abcnews.com/i/abc_live4@136330/index_500_av-p.m3u8?sd=10&rebase=on";
        	playerView.setVideoUrl(lastUrl);
        	//playerView.setVideoUrl("http://abclive.abcnews.com/i/abc_live4@136330/master.m3u8");
        	return true;
        }
        else if (id == R.id.folgers)
        {
        	lastUrl = "http://cdnbakmi.kaltura.com/p/243342/sp/24334200/playManifest/entryId/0_uka1msg4/flavorIds/1_vqhfu6uy,1_80sohj7p/format/applehttp/protocol/http/a.m3u8";
        	playerView.setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.vod_alt_audio)
        {
        	lastUrl = "http://pa-www.kaltura.com/content/shared/erank/multi_audio.m3u8";
        	playerView.setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.bipbop)
        {
        	lastUrl = "http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8";
        	playerView.setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.aes_vod)
        {
        	lastUrl = "http://live.cdn.antel.net.uy/test/hls/teststream1.m3u8";
        	playerView.setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.seekFwd)
        {
        	playerView.seek(15000);
        	return true;
        }
        else if (id == R.id.seekBwd)
        {
        	playerView.seek(-15000);
        	return true;
        }
        else if (id == R.id.testUrl)
        {
        	//playerView.setVideoUrl("https://dl.dropboxusercontent.com/u/41430608/TestStream/index_500_av-p.m3u8");
        	lastUrl = "http://www.djing.com/tv/live.m3u8";
        	playerView.setVideoUrl(lastUrl);
        	return true;
        }
        else if (id == R.id.play_pause)
        {
        	playerView.pause();
        }
        else if (id == R.id.quality_up)
        {
        	playerView.incrementQuality();
        }
        else if (id == R.id.quality_down)
        {
        	playerView.decrementQuality();
        }
        else if (id == R.id.audio_up)
        {
        	playerView.softSwitchAudioTrack(curAltAudioTrack + 1);
        }
        else if (id == R.id.audio_down)
        {
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

        	// set up a dialog window
        	alertDialogBuilder
        		.setCancelable(false)
        		.setPositiveButton("OK", new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						// get user input and set it to result
						lastUrl = input.getText().toString();
						playerView.setVideoUrl(lastUrl);
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

	@Override
	public void onSubtitleText(double startTime, double length, String buffer) {
		Log.i("VideoPlayer.onSubtitleText", "Start: " + startTime + " | Length: " + length + " | " + buffer);

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
		Log.i("VideoPlayer.onAudioSwitchingEnd", "Quaity Changed to " + newTrackIndex);
		
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

	@Override
	public void OnQualityTracksList(List<QualityTrack> list, int defaultTrackIndex) {
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Count = " + list.size());
		Log.i("VideoPlayer.onAlternateAudioAvailable", "Default = " + defaultTrackIndex);
		for (int i = 0; i < list.size(); ++i)
			LogQualityTrack(list.get(i));
	}

	@Override
	public void onQualitySwitchingStart(int oldTrackIndex, int newTrackIndex) {
		Log.i("VideoPlayer.onQualitySwitchingStart", "Quaity Changing from  " + oldTrackIndex + " to " + newTrackIndex);
		
	}

	@Override
	public void onQualitySwitchingEnd(int newTrackIndex) {
		curQualityLevel = newTrackIndex;
		Log.i("VideoPlayer.onQualitySwitchingEnd", "Quaity Changed to " + newTrackIndex);
		
	}
}
