package com.kaltura.hlsplayersdk.cache;

import java.io.IOException;

import android.util.Log;

import com.squareup.okhttp.Callback;
import com.squareup.okhttp.Request;
import com.squareup.okhttp.Response;

public class SegmentCacheEntry implements Callback {
	public String uri;
	public byte[] data;
	public boolean running;
	public long lastTouchedMillis;
	public long forceSize = -1;

	// If >= 0, ID of a crypto context on the native side.
	protected int cryptoHandle = -1;

	// All bytes < decryptHighWaterMark are descrypted; all >=  are still 
	// encrypted. This allows us to avoid duplicating every segment.
	protected long decryptHighWaterMark = 0;
	
	public static native int allocAESCryptoState(byte[] key, byte[] iv);
	public static native void freeCryptoState(int id);
	public static native long decrypt(int cryptoHandle, byte[] data, long start, long length);

	public boolean hasCrypto()
	{
		return (cryptoHandle != -1);
	}

	public void setCryptoHandle(int handle)
	{
		cryptoHandle = handle;
	}

	public void ensureDecryptedTo(long offset)
	{
		if(cryptoHandle == -1)
			return;
		
		//Log.i("HLS Cache", "Decrypting to " + offset);
		//Log.i("HLS Cache", "  first byte = " + data[0]);
		long delta = offset - decryptHighWaterMark;
		//Log.i("HLS Cache", "  delta = " + delta + " | HighWaterMark = " + decryptHighWaterMark);
		if (delta > 0)
			decryptHighWaterMark = decrypt(cryptoHandle, data, decryptHighWaterMark, delta);
		//Log.i("HLS Cache", "Decrypted to " + decryptHighWaterMark);
		//Log.i("HLS Cache", "  first byte = " + data[0]);
	}

	public boolean isFullyDecrypted()
	{
		return (decryptHighWaterMark == data.length);
	}
	
	private SegmentCachedListener mSegmentCachedListener = null;
	public void registerSegmentCachedListener(SegmentCachedListener listener)
	{
		mSegmentCachedListener = listener;
	}

	@Override
	public void onFailure(Request arg0, IOException arg1) {
		Log.e("HLS Cache", "Failed to download '" + uri + "'! " + arg1.toString());
		if (mSegmentCachedListener != null)
			mSegmentCachedListener.onSegmentFailed(uri, arg1);
	}
	
	@Override
	public void onResponse(Response response) throws IOException {
		if (!response.isSuccessful()) throw new IOException("Unexpected code " + response);
		
		Log.i("HLS Cache", "Got " + uri);
		HLSSegmentCache.store(uri, response.body().bytes());
		if (mSegmentCachedListener != null)
			mSegmentCachedListener.onSegmentCompleted(uri);
	}
}
