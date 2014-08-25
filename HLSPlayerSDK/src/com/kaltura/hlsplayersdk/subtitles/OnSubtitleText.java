package com.kaltura.hlsplayersdk.subtitles;

public interface OnSubtitleText {
	void onSubtitleText(double startTime, double length, String buffer);
}
