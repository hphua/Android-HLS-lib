package com.kaltura.hlsplayersdk.subtitles;

import java.util.Vector;

import com.kaltura.hlsplayersdk.manifest.ManifestParser;
import com.kaltura.hlsplayersdk.manifest.ManifestPlaylist;

import android.util.Log;

public class SubtitleHandler {

	private ManifestParser mManifest;
	private double mLastTime = 0;
	
	public SubtitleHandler(ManifestParser baseManifest)
	{
		mManifest = baseManifest;
	}
	
	public boolean hasSubtitles()
	{
		boolean rval = mManifest.subtitles.size() > 0;
		if (!rval) rval = mManifest.subtitlePlayLists.size() > 0;			
		return rval;
	}
	
	public void update(double time, int language)
	{
		SubTitleParser stp = getParserForTime(time, language);
		
		if (stp != null)
		{

			Vector<TextTrackCue> cues = stp.getCuesForTimeRange(mLastTime, time);
			mLastTime = time;
			
			for (int i = 0; i < cues.size(); ++i)
			{
				TextTrackCue cue = cues.get(i);
				Log.i("TextCue", "Start: " + cue.startTime + " | End: " + cue.endTime + " | " + cue.buffer);
			}
		}
	}
	
	private SubTitleParser getParserForTime(double time, int language)
	{
		ManifestParser mp = null;
		if (mManifest.subtitlePlayLists.size() > language)
		{
			
			ManifestPlaylist mpl = mManifest.subtitlePlayLists.get(language);
			if (mpl.manifest.subtitles.size() > 0)
				mp = mpl.manifest;
		}
		else if (mManifest.subtitles.size() > 0)
		{
			mp = mManifest;
		}
		
		for (int i = 0; i < mp.subtitles.size(); ++i)
		{
			SubTitleParser stp = mp.subtitles.get(i);
			if (stp != null)
			{
				if (time >= stp.startTime && time <= stp.endTime)
				{
					return stp;
				}
			}
		}
		return null;

	}
}
