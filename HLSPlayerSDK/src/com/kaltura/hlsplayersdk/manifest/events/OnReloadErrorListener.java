package com.kaltura.hlsplayersdk.manifest.events;

import java.net.URL;

public interface OnReloadErrorListener {
	void onReloadError(URL loader, String msg);
}
