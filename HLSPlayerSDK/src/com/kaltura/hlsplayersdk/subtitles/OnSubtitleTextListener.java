package com.kaltura.hlsplayersdk.subtitles;

public interface OnSubtitleTextListener {
	void onSubtitleText(double startTime, double length, String buffer);
}
