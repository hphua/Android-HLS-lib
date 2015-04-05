package com.example.videoplayer;

import android.util.Log;
import android.view.View;

import java.util.Random;
import java.util.Vector;

/**
 * Utility to soak test the player with many random actions (open, seek, change
 * quality level, choose alt audio, etc.) occasionally doing them very quickly.
 *
 * Useful for catching race conditions.
*/
public class SoakerRunnable implements Runnable {
    private VideoPlayerActivity videoPlayerActivity;
    public Vector<String> urls = null;

    private int mNextSoakDelay = 5000;
    private boolean m10pctSoakDelay = false;

    public SoakerRunnable(VideoPlayerActivity videoPlayerActivity) {
        this.videoPlayerActivity = videoPlayerActivity;
    }

    public void run() {

        final long seed = System.currentTimeMillis();
        Log.i("Soak", "Seed=" + seed);
        final Random rand = new Random(seed);

        urls = new Vector<String>();
        urls.add("http://www.kaltura.com/p/0/playManifest/entryId/1_0i2t7w0i/format/applehttp");
        urls.add("http://abclive.abcnews.com/i/abc_live4@136330/master.m3u8");
        urls.add("http://cdnbakmi.kaltura.com/p/243342/sp/24334200/playManifest/entryId/0_uka1msg4/flavorIds/1_vqhfu6uy,1_80sohj7p/format/applehttp/protocol/http/a.m3u8");
        urls.add("http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8");
        if (videoPlayerActivity.playerView.AllowAllProfiles())
        {
            urls.add("http://pa-www.kaltura.com/content/shared/erank/multi_audio.m3u8");
            urls.add("http://public.infozen.cshls.lldns.net/infozen/public/public.m3u8");
        }

        while (videoPlayerActivity.runSoak) {

            videoPlayerActivity.runOnUiThread(
                    new Runnable() {
                          public void run() {
                              int option = (int) (rand.nextDouble() * 9);
                              Log.i("Soak", "Seed=" + seed);
                              switch (option) {
                                  case 0:
                                      int i = (int) (rand.nextDouble() * urls.size());
                                      Log.i("VideoPlayer Soak", "Playing Index (" + i + ") ");

                                      videoPlayerActivity.lastUrl = urls.get(i);
                                      videoPlayerActivity.setTitle(" -----> Play " + videoPlayerActivity.lastUrl);
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.setVideoUrl(videoPlayerActivity.lastUrl);
                                      mNextSoakDelay = 5000;
                                      break;
                                  case 1:
                                      videoPlayerActivity.setTitle(" -----> Seek Fwd");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.setVisibility(View.VISIBLE);
                                      videoPlayerActivity.playerView.seek(videoPlayerActivity.mLastTimeMS + (int) (rand.nextDouble() * 15000));
                                      mNextSoakDelay = 3000;
                                      break;
                                  case 2:
                                      videoPlayerActivity.setTitle(" -----> Seek Bwd");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.setVisibility(View.VISIBLE);
                                      videoPlayerActivity.playerView.seek(videoPlayerActivity.mLastTimeMS - (int) (rand.nextDouble() * 15000));
                                      mNextSoakDelay = 3000;
                                      break;
                                  case 3:
                                      videoPlayerActivity.setTitle(" -----> Pause");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.pause();
                                      mNextSoakDelay = 1000;
                                      break;
                                  case 4:
                                      videoPlayerActivity.setTitle(" -----> Play");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.play();
                                      mNextSoakDelay = 1000;
                                      break;
                                  case 5:
                                      videoPlayerActivity.setTitle(" -----> Quality Up");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.incrementQuality();
                                      mNextSoakDelay = 5000;
                                      break;
                                  case 6:
                                      videoPlayerActivity.setTitle(" -----> Quality Down");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.decrementQuality();
                                      mNextSoakDelay = 5000;
                                      break;
                                  case 7:
                                      videoPlayerActivity.setTitle(" -----> Audio Track Up");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.hardSwitchAudioTrack(videoPlayerActivity.curAltAudioTrack + 1);
                                      mNextSoakDelay = 5000;
                                      break;
                                  case 8:
                                      videoPlayerActivity.setTitle(" -----> Audio Track Down");
                                      Log.i("VideoPlayer Soak", (String) videoPlayerActivity.getTitle());
                                      videoPlayerActivity.playerView.softSwitchAudioTrack(videoPlayerActivity.curAltAudioTrack - 1);
                                      mNextSoakDelay = 5000;
                                      break;
                              }
                          }
                      }
            );

            if (rand.nextDouble() < .1)
                m10pctSoakDelay = !m10pctSoakDelay;

            if (m10pctSoakDelay)
                mNextSoakDelay *= .1;

            try {
                Thread.sleep((long)(rand.nextDouble() * (double)mNextSoakDelay));
            } catch (InterruptedException ie) {
                Log.i("video run", "Video thread sleep interrupted!");
            }
        }
    }
}
