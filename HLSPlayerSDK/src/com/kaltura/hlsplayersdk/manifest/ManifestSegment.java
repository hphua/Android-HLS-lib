package com.kaltura.hlsplayersdk.manifest;

import java.nio.ByteBuffer;
import com.kaltura.hlsplayersdk.cache.HLSSegmentCache;
import com.kaltura.hlsplayersdk.cache.SegmentCacheEntry;

public class ManifestSegment extends BaseManifestItem 
{
	public ManifestSegment()
	{
		type = ManifestParser.SEGMENT;
	}
	
	public int id = 0;  // Based on the mediaSequence number
	//public String uri = "";
	public double duration;
	public String title;
	public double startTime;
	public int continuityEra;
	public int quality = 0;
	
	// Byte Range support. -1 means no byte range.
	public int byteRangeStart = -1;
	public int byteRangeEnd = -1;
	
	public ManifestSegment altAudioSegment = null;
	public int altAudioIndex = -1;
	
	public ManifestEncryptionKey key = null;

	public int cryptoId = -1;
	
	@Override
	public String toString()
	{
		StringBuilder sb = new StringBuilder();
		sb.append("id : " + id + " | ");
		sb.append("duration : " + duration + " | ");
		sb.append("title : " + title + " | ");
		sb.append("startTime : " + startTime + " | ");
		sb.append("continuityEra : " + continuityEra + " | ");
		sb.append("quality : " + quality + " | ");
		sb.append("byteRangeStart : " + byteRangeStart + " | ");
		sb.append("byteRangeEnd : " + byteRangeEnd + "\n");
		sb.append("uri : " + uri + "\n");
		sb.append("cryptoId : " + cryptoId + "\n");
		
		return sb.toString();
		
	}

	public void initializeCrypto()
	{
		// Read the key optimistically.
		ByteBuffer keyBytes = ByteBuffer.allocate(16);
		HLSSegmentCache.read(key.url, 0, 16, keyBytes);

		// Super fake IV.
		byte[] iv = new byte[16];
		for(int i=0; i<16; i++) iv[i] = 0;
		//E0A9
		iv[14] = (byte)0xe0;
		iv[15] = (byte)0xa9;

		cryptoId = SegmentCacheEntry.allocAESCryptoState(keyBytes.array(), iv);
	}

}
