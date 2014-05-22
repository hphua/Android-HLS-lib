package com.kaltura.hlsplayersdk.manifest.events;

import java.net.URL;

public interface OnLoadErrorListener {
	void onLoadError(URL loader, String msg);
}
