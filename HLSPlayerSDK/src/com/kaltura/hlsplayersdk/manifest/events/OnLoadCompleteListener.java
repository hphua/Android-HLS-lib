package com.kaltura.hlsplayersdk.manifest.events;

import java.net.URL;

import com.kaltura.hlsplayersdk.manifest.BaseManifestItem;

public interface OnLoadCompleteListener {
	public void onLoadComplete(URL loader, BaseManifestItem manifestItem);
}
