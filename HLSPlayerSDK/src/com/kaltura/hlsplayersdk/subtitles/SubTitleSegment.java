package com.kaltura.hlsplayersdk.subtitles;


import java.util.Vector;

import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;

import android.util.Log;

public class SubTitleSegment {
	public Vector<WebVTTRegion> regions = new Vector<WebVTTRegion>();
	
	enum ParseState
	{
		IDLE,
		PARSE_HEADERS,
		PARSE_CUE_SETTINGS,
		PARSE_CUE_TEXT
	}

	public Vector<TextTrackCue> textTrackCues = new Vector<TextTrackCue>();
	public double segmentTimeWindowStart = -1;
	public double segmentTimeWindowDuration = -1;
	public int id = 0;
	
	private String _url;
	private boolean _isLoaded = false;
	private double _mpegts = 0;
	private double _localts = 0;
	private double _tsmod = 0;
	
	
	
	public SubTitleSegment()
	{
		
	}
	
	public SubTitleSegment(String url)
	{
		_url = url;
	}
	
	public void setUrl(String url)
	{
		_url = url;
	}
	
	public boolean inPrecacheWindow(double time, double windowSize)
	{
		if ( time > ((segmentTimeWindowStart + segmentTimeWindowDuration) - windowSize))
		{
			return true;
		}
		return false;
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
	
	public boolean isLoaded()
	{
		return _isLoaded;
	}
	
	public void load()
	{
		String file = HLSSegmentCache.readFileAsString(_url);
		parse(file);
	}
	
	public void precache()
	{
		HLSSegmentCache.precache(_url, -1);
	}
	
	public void setTimeWindowStart(double time)
	{
		segmentTimeWindowStart = time;
		
		// Now we need to modify the cues so that they match the start time
		// We'll subtract the difference between _tsmod and start time from the start and end of each segment
		
		double diff = _tsmod - segmentTimeWindowStart;
		for (int i = 0; i < textTrackCues.size(); ++i)
		{
			textTrackCues.get(i).startTime -= diff;
			textTrackCues.get(i).endTime -= diff;
		}
		
	}
	
	
	public void parse(String input)
	{
		// Normalize line endings.
		input = input.replace("\r\n", "\n");
		
		// split into array
		String[] lines = input.split("\n");
		
		if (lines.length < 1 || lines[0].indexOf("WEBVTT") == -1)
		{
			Log.i("SubTitleParser.parse", "Not a valid WEBVTT file " + _url);
			postSubtitleParseComplete(this);
			return;
		}
		
		ParseState state = ParseState.PARSE_HEADERS;
		TextTrackCue textTrackCue = null;

		// set up our offset
		double tsdiff = 0.0f;

		
		// Process each line
		
		for (int i = 1; i < lines.length; ++i)
		{
			String line = lines[i];
			if (line.equals(""))
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
				if (line.indexOf("X-TIMESTAMP-MAP") == 0)
				{
					parseTimeStampMap(line);
					tsdiff = _tsmod - segmentTimeWindowStart;
				}
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
				textTrackCue.startTime += (_tsmod - tsdiff);
				textTrackCue.endTime += (_tsmod - tsdiff);
				textTrackCue.buffer += line;
				state = ParseState.PARSE_CUE_TEXT;
				break;
				
			case PARSE_CUE_TEXT:
				if (textTrackCue.text.length() > 0) textTrackCue.text += "\n";
				textTrackCue.text += line;
				textTrackCue.buffer += "\n" + line;
				break;
			}
			
		}
		
		// And one last cue, just in case there wasn't an empty line
		if (textTrackCue != null) textTrackCues.add(textTrackCue);
		
		_isLoaded = true;
		
		postSubtitleParseComplete(this);
	}
	
	private void parseTimeStampMap(String input)
	{
		long mpegts = 0;
		String[] keyPair = input.split("=");
		if (keyPair[0].equals("X-TIMESTAMP-MAP"))
		{
			String[] values = keyPair[1].split(",");
			
			for (int i = 0; i < values.length; ++i)
			{
				if (values[i].startsWith("MPEGTS:"))
				{
					mpegts = Long.parseLong(values[i].substring("MPEGTS:".length()));
					
				}
				else if (values[i].startsWith("LOCAL:"))
				{
					_localts = parseTimeStamp(values[i].substring("LOCAL:".length()));
				}
			}
		}
		
		Log.i("SubTitleParser.parseTimeStampMap", "Input: " + input + " | MPEGTS=" + mpegts + ", LOCAL=" + _localts);
		
		_tsmod = (double)(mpegts / 90000); // Convert it to seconds
		
		_tsmod -= _localts; // Since _localts and _tsmod should be equivalent times, subtract _localts from _tsmod so that we know our offset
	
	}
	
	public static double parseTimeStamp(String input)
	{
		// Time string parsed from format 00:00:00.000 and similar
		int hours = 0;
		int minutes = 0;
		int seconds = 0;
		int milliseconds = 0;
		String[] units = input.split(":");
		

		String secondValues = "";
		
		if (units.length < 3)
		{
			minutes = Integer.parseInt(units[0]);
			secondValues = units[1];
		}
		else
		{
			hours = Integer.parseInt(units[0]);
			minutes = Integer.parseInt(units[1]);
			secondValues = units[2];
		}
		
		String[] secondUnits = secondValues.split("[.]");
		
		seconds = Integer.parseInt(secondUnits[0]);
		if (secondUnits.length > 1) milliseconds = Integer.parseInt(secondUnits[1]);
		
		return (double)(hours * 60 * 60 + minutes * 60 + seconds) + ((double)milliseconds / (double)1000);
	}
	
	// Event Listeners
	public void setOnParseCompleteListener(OnSubtitleParseCompleteListener listener)
	{
		mOnParseCompleteListener = listener;
	}
	private OnSubtitleParseCompleteListener mOnParseCompleteListener;
	
	public void postSubtitleParseComplete(final SubTitleSegment parser)
	{
		if (mOnParseCompleteListener != null) mOnParseCompleteListener.onSubtitleParserComplete(parser);
	}
	

}
