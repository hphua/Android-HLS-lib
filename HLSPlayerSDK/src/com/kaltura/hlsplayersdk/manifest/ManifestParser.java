package com.kaltura.hlsplayersdk.manifest;


import java.net.MalformedURLException;
import com.loopj.android.http.*;
import java.net.URL;
import java.util.Vector;

import android.util.Log;
import android.util.EventLog.Event;
import android.view.View.OnClickListener;

import com.kaltura.hlsplayersdk.URLLoader;
import com.kaltura.hlsplayersdk.URLLoader.DownloadEventListener;
import com.kaltura.hlsplayersdk.subtitles.*;
import com.kaltura.hlsplayersdk.manifest.events.*;

public class ManifestParser implements OnParseCompleteListener, URLLoader.DownloadEventListener {
	public static final String DEFAULT = "DEFAULT";
	public static final String AUDIO = "AUDIO";
	public static final String VIDEO = "VIDEO";
	public static final String SUBTITLES = "SUBTITLES";
	public static final String SEGMENT = "SEGMENT";
	
	
	public String type = DEFAULT;
	public int version;
	public String baseUrl;
	public String fullUrl;
	public int mediaSequence;
	public boolean allowCache;
	public double targetDuration;
	public boolean streamEnds = false;
	public Vector<ManifestPlaylist> playLists = new Vector<ManifestPlaylist>();
	public Vector<ManifestStream> streams = new Vector<ManifestStream>();
	public Vector<ManifestSegment> segments = new Vector<ManifestSegment>();
	public Vector<ManifestPlaylist> subtitlePlayLists = new Vector<ManifestPlaylist>();
	public Vector<SubTitleParser> subtitles = new Vector<SubTitleParser>();
	public Vector<ManifestEncryptionKey> keys = new Vector<ManifestEncryptionKey>();
	
	public Vector<URLLoader> manifestLoaders = new Vector<URLLoader>();
	public Vector<ManifestParser> manifestParsers = new Vector<ManifestParser>();
	//private URLLoader manifestReloader = null;
	
	public int continuityEra = 0;
	private int _subtitlesLoading = 0;
	
	@Override
	public String toString()
	{
		StringBuilder sb = new StringBuilder();
		sb.append("type : " + type + "\n");
		sb.append("version : " + version + "\n");
		sb.append("baseUrl : " + baseUrl + "\n");
		sb.append("fullUrl : " + fullUrl + "\n");
		sb.append("mediaSequence : " + mediaSequence + "\n");
		sb.append("allowCache : " + allowCache + "\n");
		sb.append("targetDuration : " + targetDuration + "\n");
		sb.append("streamEnds : " + streamEnds + "\n");
		
		for (int i = 0; i < segments.size(); ++i)
		{
			sb.append("---- ManifestSegment ( " + i  + " )\n");
			sb.append(segments.get(i).toString());
		}
		
		
		for (int i = 0; i < streams.size(); ++i)
		{
			sb.append("---- Stream ( " + i  + " )\n");
			sb.append(streams.get(i).toString());
		}
		
		return sb.toString();
	}
	
	public void dumpToLog()
	{
		Log.i("ManifestParser.dumpToLog", this.toString());
	}
	
	public static String getNormalizedUrl( String baseUrl, String uri)
	{
		return ( uri.substring(0, 5).equals("http:") || uri.substring(0, 6).equals("https:") || uri.substring(0, 5).equals("file:")) ? uri : baseUrl + uri;
	}
	
	public static <T> T as(Class<T> t, Object o) {
		  return t.isInstance(o) ? t.cast(o) : null;
		}

	private Object lastHint = null;

	public void parse(String input, String _fullUrl)
	{
		lastHint = null;
		fullUrl = _fullUrl;
		baseUrl = _fullUrl.substring(0, _fullUrl.lastIndexOf('/') + 1);
		
		// Normalize line endings
		input = input.replace("\r\n", "\n");
		
		// split into an array
		String [] lines = input.split("\n");
		
		// process each line
		
		int nextByteRangeStart = 0;
		
		int i = 0;
		for ( i = 0; i < lines.length; ++i)
		{
			String curLine = lines[i];
			String curPrefix = curLine.substring(0, 1);
			
			// Ignore empty lines;
			if (curLine.length() == 0) continue;
			
			if (!curPrefix.endsWith("#") && curLine.length() > 0)
			{
				// Specifying a media file, note it
				if ( !type.equals(SUBTITLES ))
				{
					String targetUrl = getNormalizedUrl(baseUrl, curLine);
// TODO - lastHint!!!
					ManifestSegment segment = as(ManifestSegment.class, lastHint);
					if (segment != null && segment.byteRangeStart != -1)
					{
						// Append akamai ByteRange properties to URL
						String urlPostFix = targetUrl.indexOf( "?" ) == -1 ? "?" : "&";
						targetUrl += urlPostFix + "range=" + segment.byteRangeStart + "-" + segment.byteRangeEnd;
					}
//TODO - figure out what to do about lastHint, because it isn't going to work the way it's being used
					//lastHint.getClass().getField("uri").set(lastHint, targetUrl);
					
					
					BaseManifestItem mi = as(BaseManifestItem.class, lastHint);
					if (mi != null)
						mi.uri = targetUrl;
					else
					{
						Log.e("ManifestParser.parse", "UnknownType. Can't Set URI: " + lastHint.toString());
					}
						
				}
				else
				{
// TODO - SubTitleParser!!!
					//((SubTitleParser)lastHint).load( getNormalizedUrl ( baseUrl, curLine) );
				}
				continue;
			}
			
			// Otherwise, we are processing a tag.
			int colonIndex = curLine.indexOf(':');
			String tagType = colonIndex > -1 ? curLine.substring(1, colonIndex) : curLine.substring(1);
			String tagParams = colonIndex > -1 ? curLine.substring( colonIndex + 1) : "";
			
			if (tagType.equals("EXTM3U")) 
			{
				if (i != 0)
					Log.w("ManifestParser.parse", "Saw EXTM3U out of place! Ignoring...");
			}
			else if (tagType.equals("EXT-X-TARGETDURATION")) 
			{
				targetDuration = Integer.parseInt(tagParams);
			}
			else if (tagType.equals("EXT-X-ENDLIST"))
			{
				// This will only show up in live streams if the stream is over.
				// This MUST (according to the spec) show up in any stream in which no more
				//     segments will be made available.
				streamEnds = true;
			}
			else if (tagType.equals("EXT-X-KEY"))
			{
//				if (keys.size() > 0) keys.get(keys.size() - 1).endSegmentId = segments.size() - 1;
//				ManifestEncryptionKey key = ManifestEncryptionKey.fromParams(tagParams);
//				key.startSegmentId = segments.size();
//				keys.add(key);
			}
			else if (tagType.equals("EXT-X-VERSION"))
			{
				version = Integer.parseInt(tagParams);
			}
			else if (tagType.equals("EXT-X-MEDIA-SEQUENCE"))
			{
				mediaSequence = Integer.parseInt(tagParams);
			}
			else if (tagType.equals("EXT-X-ALLOW-CACHE"))
			{
				allowCache = tagParams.equals("YES") ? true : false;
			}
			else if (tagType.equals("EXT-X-MEDIA"))
			{
				if ( tagParams.indexOf( "TYPE=AUDIO" ) != -1 )
				{
					ManifestPlaylist playList = ManifestPlaylist.fromString( tagParams ); 
					playList.uri = getNormalizedUrl( baseUrl, playList.uri );
					playLists.add( playList );
				}
				else if ( tagParams.indexOf( "TYPE=SUBTITLES" ) != -1 )
				{
					ManifestPlaylist subtitleList = ManifestPlaylist.fromString( tagParams );
					subtitleList.uri = getNormalizedUrl( baseUrl, subtitleList.uri );
					subtitlePlayLists.add( subtitleList );
				}
				else Log.w("ManifestParser.parse", "Encountered " + tagType + " tag that is not supported, ignoring." );				
			}
			else if (tagType.equals("EXT-X-STREAM-INF"))
			{
				streams.add(ManifestStream.fromString(tagParams));
// TODO: Last Hint
				lastHint = streams.get(streams.size() - 1);
			}
			else if (tagType.equals("EXTINF"))
			{
				if ( type.equals(SUBTITLES ))
				{
					//TODO: SUBTITLES!!!
//					SubTitleParser subTitle = new SubTitleParser();
//					subTitle.addEventListener( Event.COMPLETE, onSubtitleLoaded );
//					subtitles.add( subTitle );
//					lastHint = subTitle;
//					_subtitlesLoading++;
				}
				else
				{
					lastHint = new ManifestSegment();
					segments.add((ManifestSegment)lastHint);
					lastHint = segments.get(segments.size()-1);
					String [] valueSplit = tagParams.split(",");
					((ManifestSegment)lastHint).duration =  Double.parseDouble(valueSplit[0]);
					((ManifestSegment)lastHint).continuityEra = continuityEra;
					if(valueSplit.length > 1)
					{
						((ManifestSegment)lastHint).title = valueSplit[1];
					}
				}				
			}
			else if (tagType.equals("EXT-X-BYTERANGE"))
			{
				ManifestSegment hintAsSegment = as(ManifestSegment.class, lastHint);
				if ( hintAsSegment == null ) break;
				String [] byteRangeValues = tagParams.split("@");
				hintAsSegment.byteRangeStart = byteRangeValues.length > 1 ? Integer.parseInt( byteRangeValues[ 1 ] ) : nextByteRangeStart;
				hintAsSegment.byteRangeEnd = hintAsSegment.byteRangeStart + Integer.parseInt( byteRangeValues[ 0 ] );
				nextByteRangeStart = hintAsSegment.byteRangeEnd + 1;
			}
			else if (tagType.equals("EXT-X-DISCONTINUITY"))
			{
				++continuityEra;
			}
			else if (tagType.equals("EXT-X-PROGRAM-DATE-TIME"))
			{
				
			}
			else
			{
				Log.w("ManifestParser.parse", "Unknown tag '" + tagType + "', ignoring...");
			}
//			if (lastHint == null)
//				Log.i("Parse.lastHint", "null");
//			else
//				Log.i("Parse.lastHint", lastHint.toString());
			
		}
		
		// Process any other manifests referenced
		//boolean pendingManifests = false;
		Vector<BaseManifestItem> manifestItems = new Vector<BaseManifestItem>();
		manifestItems.addAll(streams);
		manifestItems.addAll(playLists);
		manifestItems.addAll(subtitlePlayLists);
		
		for (int k = 0; k < manifestItems.size(); ++k)
		{
			BaseManifestItem curItem = manifestItems.get(k);
			if (curItem.uri.lastIndexOf("m3u8") != -1)
			{
				// Request and parse the manifest.
				addItemToManifestLoader(curItem);
			}
		}
		
		double timeAccum = 0.0;
		for (int m = 0; m < segments.size(); ++m)
		{
			segments.get(m).id = mediaSequence + m; // set the id based on the media sequence
			segments.get(m).startTime = timeAccum;
			timeAccum += segments.get(m).duration;
		}
		
		if (manifestLoaders.size() == 0 && this.mOnParseCompleteListener != null)
			mOnParseCompleteListener.onParserComplete(this);
			
	}
	
	private void verifyManifestItemIntegrity()
	{
		// work through the streams and remove any broken ones
		for (int i = streams.size() - 1; i >= 0; --i)
		{
			if (streams.get(i).manifest == null)
				streams.remove(i);
		}
		
		for (int i = playLists.size() - 1; i >= 0; --i)
		{
			if (playLists.get(i).manifest == null)
				playLists.remove(i);
		}
	}
	
	private void addItemToManifestLoader(BaseManifestItem item)
	{
		URLLoader manifestLoader = new URLLoader(this, item);
		manifestLoaders.add(manifestLoader);
		manifestLoader.get(item.uri);
	}
	
	@Override
	public void onDownloadFailed(URLLoader loader, String response) {
		if (loader.manifestItem != null)
		{
			Log.w("ManifestParser.onManifestError", "ERROR loading maifest " + response);
			manifestLoaders.remove(loader);
			announceIfComplete();
		}
		else
		{
			
		}
		if (mReloadEventListener != null) mReloadEventListener.onReloadFailed(this);
	}

	@Override
	public void onDownloadComplete(URLLoader loader, String response) {
		
		if (loader.manifestItem != null) // this is a load of a submanifest
		{
			String resourceData = response;
			BaseManifestItem manifestItem = loader.manifestItem;
			manifestLoaders.remove(loader);
			
			ManifestParser parser = new ManifestParser();
			parser.type = manifestItem.type;
			manifestItem.manifest = parser;
			manifestParsers.add(parser);
	
			parser.setOnParseCompleteListener(this);
			parser.parse(resourceData, getNormalizedUrl(baseUrl, manifestItem.uri));
		}
		else // this is a reload!
		{
			String resourceData = response;
			if (mOnParseCompleteListener == null) setOnParseCompleteListener(this);
			parse(resourceData, fullUrl);
		}
		
	}

	@Override
	public void onParserComplete(ManifestParser parser)
	{
		if (parser == this && mReloadEventListener != null) // We're reloading
		{
			mReloadEventListener.onReloadComplete(this);
		}
		else
		{
			manifestParsers.remove(parser);
			announceIfComplete();
		}
	}

	public void reload(ManifestParser manifest)
	{
		// When the URLLoader finishes, it should set the parseComplete listener to *this*, and
		// when that completes, it should call the reloadCompleteListener
		fullUrl = manifest.fullUrl;
		URLLoader manifestLoader = new URLLoader(this, null);
		manifestLoader.get(fullUrl);
	}
	
	private void onSubtitleLoaded(Event e)
	{
		Log.i("ManifestParser.onSubtitleLoaded", "SUBTITLE LOADED");
		_subtitlesLoading--;
		announceIfComplete();
	}
	
	private void announceIfComplete()
	{
		if (_subtitlesLoading == 0 && manifestParsers.size() == 0 && manifestLoaders.size() == 0)
		{
			verifyManifestItemIntegrity();
			//PostEvent (COMPLETE);
			if (mOnParseCompleteListener != null) mOnParseCompleteListener.onParserComplete(this);
		}
	}
	
	
	public interface ReloadEventListener
	{
		void onReloadComplete(ManifestParser parser);
		void onReloadFailed(ManifestParser parser);
	}
	
	private ReloadEventListener mReloadEventListener = null;
	
	public void setReloadEventListener(ReloadEventListener listener)
	{
		mReloadEventListener = listener;
	}
	
	// Event Listeners
	public void setOnParseCompleteListener(OnParseCompleteListener listener)
	{
		mOnParseCompleteListener = listener;
	}
	private OnParseCompleteListener mOnParseCompleteListener;
//	
//	
//	public void setOnLoadErrorListener(OnLoadErrorListener listener)
//	{
//		mOnLoadErrorListener = listener;
//	}
//	private OnLoadErrorListener mOnLoadErrorListener;
//	
//	public void setOnReloadErrorListener(OnReloadErrorListener listener)
//	{
//		mOnReloadErrorListener = listener;
//	}
//	
//	private OnReloadErrorListener mOnReloadErrorListener; 
//	
//	public void setOnReloadCompleteListener(OnReloadCompleteListener listener)
//	{
//		mOnReloadCompleteListener = listener;
//	}
//	
//	private OnReloadCompleteListener mOnReloadCompleteListener;
}
