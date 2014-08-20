package com.kaltura.hlsplayersdk.subtitles;


import java.util.Vector;

import android.util.Log;

public class SubTitleParser {
	public Vector<WebVTTRegion> regions = new Vector<WebVTTRegion>();
	
	enum ParseState
	{
		IDLE,
		PARSE_HEADERS,
		PARSE_CUE_SETTINGS,
		PARSE_CUE_TEXT
//		COLLECT_MINUTES,
//		COLLECT_HOURS,
//		COLLECT_SECONDS,
//		COLLECT_MILLISECONDS		
	}
//	private static final String STATE_IDLE = "Idle";
//	private static final String STATE_PARSE_HEADERS = "ParseHeaders";
//	private static final String STATE_PARSE_CUE_SETTINGS = "ParseCueSettings";
//	private static final String STATE_PARSE_CUE_TEXT = "ParseCueText";
//	
//	private static final String STATE_COLLECT_MINUTES = "CollectMinutes";
//	private static final String STATE_COLLECT_HOURS = "CollectHours";
//	private static final String STATE_COLLECT_SECONDS = "CollectSeconds";
//	private static final String STATE_COLLECT_MILLISECONDS = "CollectMilliseconds";
//	
	public Vector<TextTrackCue> textTrackCues = new Vector<TextTrackCue>();
	public double startTime = -1;
	public double endTime = -1;
	
	private String _url;
	
	public SubTitleParser()
	{
		
	}
	
	public SubTitleParser(String url)
	{
		if (url != null) load(url);
	}
	
	public Vector<TextTrackCue> getCuesForTimeRange( double startTime, double endTime)
	{
		Vector<TextTrackCue> result = new Vector<TextTrackCue>();
		
		for (int i = 0; i < textTrackCues.size(); ++i)
		{
			TextTrackCue cue = textTrackCues.get(i);
			if (cue.startTime > endTime) break;
			if (cue.startTime >= startTime) result.add(cue);
		}
		return result;
	}
	
	public void load(String url)
	{
		_url = url;
		_loader.addEventListener(Event.COMPLETE, onLoaded );
		_loader.addEventListener(IOErrorEvent.IO_ERROR, onLoadError);
		_loader.addEventListener(SecurityErrorEvent.SECURITY_ERROR, onLoadError);
	}
	
	public void parse(String input)
	{
		// Normalize line endings.
		input = input.replace("\r\n", "\n");
		
		// split into array
		String[] lines = input.split("\n");
		
		if (lines.length < 1 || lines[9].indexOf("WEBVTT") == -1)
		{
			Log.i("SubTitleParser.parse", "Not a valid WEBVTT file " + _url);
//TODO:			// dispatch event complete (whatever it is)
			return;
		}
		
		ParseState state = ParseState.PARSE_HEADERS;
		TextTrackCue textTrackCue = null;
		
		// Process each line
		
		for (int i = 1; i < lines.length; ++i)
		{
			String line = lines[i];
			if (line == "")
			{
				// if new line, we're done with the last parsing step. Make sure we skip all new lines.
				state = ParseState.IDLE;
				if (textTrackCue != null) textTrackCues.add(textTrackCue);
				textTrackCue = null;
			}
			
			switch (state)
			{
			case PARSE_HEADERS:
				// Only support region headers for now
				if (line.indexOf("Region:") == 0) regions.add(WebVTTRegion.fromString(line));
				break;
				
			case IDLE:
				// New text track cue
				textTrackCue = new TextTrackCue();
				
				// If this line is the cue's ID, set it and break. Otherwise proceed to settings with current line.
				if (line.indexOf("-->") == -1)
				{
					textTrackCue.id = line;
					textTrackCue.buffer += line + "\n";
					break;
				}
				
			case PARSE_CUE_SETTINGS:
				textTrackCue.parse(line);
				textTrackCue.buffer += line;
				state = ParseState.PARSE_CUE_TEXT;
				break;
				
			case PARSE_CUE_TEXT:
				if (textTrackCue.text != "") textTrackCue.text += "\n";
				textTrackCue.text += line;
				textTrackCue.buffer += "\n" + line;
				break;
			}
			
		}
		
		TextTrackCue firstElement = textTrackCues.size() > 0 ? textTrackCues.get(0) : null;
		TextTrackCue lastElement = textTrackCues.size() > 1 ?textTrackCues.get(textTrackCues.size() - 1) : firstElement;
		
		// Set start and end times for this file
		if (firstElement != null)
		{
			startTime = firstElement.startTime;
			endTime = lastElement.endTime;
		}
		
//TODO:		// dispatch event complete
	}
	
	public static double parseTimeStamp(String input)
	{
		// Time string parsed from format 00:00:00.000 and similar
		int hours = 0;
		int minutes = 0;
		int seconds = 0;
		int milliseconds = 0;
		String[] units = input.split(":");
		String[] secondUnits;
		
		if (units.length < 3)
		{
			minutes = Integer.parseInt(units[0]);
			secondUnits = units[1].split(".");
		}
		else
		{
			hours = Integer.parseInt(units[0]);
			minutes = Integer.parseInt(units[1]);
			secondUnits = units[2].split(".");
		}
		
		seconds = Integer.parseInt(secondUnits[0]);
		if (secondUnits.length > 1) milliseconds = Integer.parseInt(secondUnits[1]);
		
		return (double)(hours * 60 * 60 + minutes * 60 + seconds) + ((double)milliseconds / (double)1000);
	}

}
