package com.kaltura.hlsplayersdk.subtitles;

/*
 * This event is fired once when there is a line of text available.
 * 
 * The start time and length is in seconds, and the buffer contains
 * the text to be displayed.
 * 
 * Register your handler by calling PlayerViewController.registerSubtitleTextListener()
 */

public interface OnSubtitleTextListener {
	void onSubtitleText(double startTime, double length, String buffer);
}
