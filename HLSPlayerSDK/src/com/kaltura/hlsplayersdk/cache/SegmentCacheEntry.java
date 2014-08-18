package com.kaltura.hlsplayersdk.cache;

import com.loopj.android.http.RequestHandle;

class SegmentCacheEntry {
	public String uri;
	public byte[] data;
	public RequestHandle request;
	public boolean running;
	public long lastTouchedMillis;
}
