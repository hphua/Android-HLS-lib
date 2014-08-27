package com.kaltura.hlsplayersdk.manifest.events;


/*
 * This event returns the languages and the index of the default language of a stream
 * that has alternate audio tracks available. Currently, this event is only fired when a stream is
 * started.
 * 
 * Register your handler by calling PlayerViewController.registerAlternateAudioAvailable()
 */

public interface OnAlternateAudioAvailableListener {
	public void onAlternateAudioAvailable(String[] languages, int defaultLanguage);
}
