package com.kaltura.hlsplayersdk.cache;

import java.io.IOException;

public interface SegmentCachedListener {
	public void onSegmentCompleted(String uri);
	public void onSegmentFailed(String uri, IOException e);
}
