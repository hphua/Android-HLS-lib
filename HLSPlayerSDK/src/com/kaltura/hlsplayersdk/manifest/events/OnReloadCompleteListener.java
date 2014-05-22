package com.kaltura.hlsplayersdk.manifest.events;

import java.net.URL;

import com.kaltura.hlsplayersdk.manifest.BaseManifestItem;

public interface OnReloadCompleteListener {
	void onReloadComplete(URL loader, BaseManifestItem manifestItem);
}
