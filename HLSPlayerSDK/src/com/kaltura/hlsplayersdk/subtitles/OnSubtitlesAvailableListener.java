package com.kaltura.hlsplayersdk.subtitles;

/*
 * This event returns the languages and the index of the default language of a stream
 * that has subtitles available. Currently, this event is only fired when a stream is
 * started.
 * 
 * Register your handler by calling PlayerViewController.registerSubtitlesAvailable()
 */

public interface OnSubtitlesAvailableListener {
	public void onSubtitlesAvailable(String[] languages, int defaultLanguage);
}
