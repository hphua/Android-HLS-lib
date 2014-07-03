package com.kaltura.hlsplayersdk;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import com.kaltura.hlsplayersdk.events.HLSPlayerEventListener;
import com.kaltura.hlsplayersdk.events.OnPlayerStateChangeListener;
import com.kaltura.hlsplayersdk.events.OnToggleFullScreenListener;
import com.kaltura.hlsplayersdk.types.PlayerStates;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ViewGroup;
import android.view.animation.BounceInterpolator;
import android.widget.RelativeLayout;

public class PlayerViewController extends RelativeLayout 
{
	private PlayerView mPlayerView;
	private Activity mActivity;
	private OnToggleFullScreenListener mFSListener;
    private HashMap<String, ArrayList<HLSPlayerEventListener>> mHLSplayerEventsMap = new HashMap<String, ArrayList<HLSPlayerEventListener>>();
    private HashMap<String, HLSPlayerEventListener> mHLSplayerEvaluatedMap = new HashMap<String, HLSPlayerEventListener>();

	
    public PlayerViewController(Context context) {
        super(context);
    }
    
    public PlayerViewController(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public PlayerViewController(Context context, AttributeSet attrs,
            int defStyle) {
        super(context, attrs, defStyle);
    }
    
    public void close()
    {
    	if (mPlayerView != null) mPlayerView.close();
    }
    
//    @Override
//    protected void onDraw(Canvas canvas) {
//        super.onDraw(canvas);
//    }

    public void setOnFullScreenListener(OnToggleFullScreenListener listener) {
        mFSListener = listener;
    }
    
    public void setPlayerViewDimensions(int width, int height, int xPadding, int yPadding) {
    	setPadding(xPadding, yPadding, 0, 0);
    	setPlayerViewDimensions( width+xPadding, height+yPadding);
    }

    public void setPlayerViewDimensions(int width, int height) {
        ViewGroup.LayoutParams lp = getLayoutParams();
        if ( lp == null ) {
        	lp = new ViewGroup.LayoutParams( width, height );
        } else {
            lp.width = width;
            lp.height = height;
        }

        this.setLayoutParams(lp);

        invalidate();
    }
    
//    public void addComponents(String partnerId, String entryId, Activity activity) {
//        String iframeUrl = host + html5Url + "?wid=_" + partnerId + "&uiconf_id=" + playerId + "&entry_id=" + entryId;
//        addComponents( iframeUrl, activity );
//    }
//
    /**
     * load given url to the player view
     * 
     * @param iframeUrl
     *            url to payer
     * @param activity
     *            bounding activity
     */
    public void addComponents(String iframeUrl, Activity activity) {
        mActivity = activity;
        LayoutParams lp = new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        lp.addRule(CENTER_VERTICAL);
        lp.addRule(CENTER_HORIZONTAL);

        mPlayerView = new PlayerView(mActivity);
        Log.w("addComponents", "Surface Holder is " + mPlayerView.getHolder());
        if(mPlayerView.getHolder() != null)
        	Log.w("addComponents", "Surface Holder is " + mPlayerView.getHolder().getSurface());
        super.addView(mPlayerView, lp);
//        mPlayerView.play();
        // video finish listener
//        mPlayerView.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
//
//            @Override
//            public void onCompletion(MediaPlayer mp) {
//                // not playVideo
//                            // playVideo();
////            				mPlayerView.setVideoPath("/storage/emulated/0/Movies/segment2_0_av.ts");
////                            mPlayerView.play();
//            }
//        });
        // disables videoView auto resize according to content dimensions
        // mPlayerView.setDimensions(width, height);
//        setPlayerListeners();
        
        ViewGroup.LayoutParams currLP = getLayoutParams();
        LayoutParams wvLp = new LayoutParams(currLP.width, currLP.height);

    }
    
    /**
     * slides with animation according the given values
     * 
     * @param x
     *            x offset to slide
     * @param duration
     *            animation time in milliseconds
     */
    public void slideView(int x, int duration) {
//        this.animate().xBy(x).setDuration(duration)
//                .setInterpolator(new BounceInterpolator());
    }
    
    public void destroy() {
        if ( mPlayerView!=null )
        {
            mPlayerView.stop();
            mPlayerView.close();
        }
    }
    
    // /////////////////////////////////////////////////////////////////////////////////////////////
    // VideoPlayerInterface methods
    // /////////////////////////////////////////////////////////////////////////////////////////////
    public boolean isPlaying() {
        return (mPlayerView != null && mPlayerView.isPlaying());
    }

    public int getDuration() {
        int duration = 0;
        if (mPlayerView != null)
            duration = mPlayerView.getDuration();

        return duration;
    }

    public String getVideoUrl() {
        String url = null;
        if (mPlayerView != null)
            url = mPlayerView.getVideoUrl();

        return url;
    }

    public void play() {
        if (mPlayerView != null) {
            mPlayerView.play();
        }
    }

    public void pause() {
        if (mPlayerView != null) {
            mPlayerView.pause();
        }
    }

    public void stop() {
        if (mPlayerView != null) {
            mPlayerView.stop();
        }
    }

    public void seek(int msec) {
        if (mPlayerView != null) {
            mPlayerView.seek(msec);
        }
    }
    
    // /////////////////////////////////////////////////////////////////////////////////////////////
    // Kaltura Player external API
    // /////////////////////////////////////////////////////////////////////////////////////////////
//    public void registerJsCallbackReady( HLSPlayerJsCallbackReadyListener listener ) {
//    	mJsReadyListener = listener;
//    }
    
//    public void sendNotification(String noteName, JSONObject noteBody) {
//        notifyHLSPlayer("sendNotification",  new String[] { noteName, noteBody.toString() });      
//    }

    public void addKPlayerEventListener(String eventName,
            HLSPlayerEventListener listener) {
        ArrayList<HLSPlayerEventListener> listeners = mHLSplayerEventsMap
                .get(eventName);
        boolean isNewEvent = false;
        if ( listeners == null ) {
            listeners = new ArrayList<HLSPlayerEventListener>();
        }
        if ( listeners.size() == 0 ) {
            isNewEvent = true;
        }
        listeners.add(listener);
        mHLSplayerEventsMap.put(eventName, listeners);
        if ( isNewEvent )
            notifyHLSPlayer("addJsListener", new String[] { eventName });
    }

    public void removeHLSPlayerEventListener(String eventName,String callbackName) {
        ArrayList<HLSPlayerEventListener> listeners = mHLSplayerEventsMap.get(eventName);
        if (listeners != null) {
            for (int i = 0; i < listeners.size(); i++) {
                if ( listeners.get(i).getCallbackName().equals( callbackName )) {
                    listeners.remove(i);
                    break;
                }
            }
            if ( listeners.size() == 0 )
                notifyHLSPlayer( "removeJsListener", new String[] { eventName });
        }
    }

    public void setKDPAttribute(String hostName, String propName, String value) {
        notifyHLSPlayer("setKDPAttribute", new String[] { hostName, propName, value });
    }
    
    public void setVideoUrl(String url)
    {
    	Log.i("PlayerViewController.setVideoUrl", url);
    	mPlayerView.setVideoUrl(url);
    }

    public void asyncEvaluate(String expression, HLSPlayerEventListener listener) {
        String callbackName = listener.getCallbackName();
        mHLSplayerEvaluatedMap.put(callbackName, listener);
        notifyHLSPlayer("asyncEvaluate", new String[] { expression, callbackName });
    }
    
    
    private void notifyHLSPlayer(final String action, final String[] eventValues) {
    	
//        mActivity.runOnUiThread(new Runnable() {
//            @Override
//            public void run() {
//                String values = "";
//                if (eventValues != null) {
//                    values = TextUtils.join("', '", eventValues);
//                }
//                mWebView.loadUrl("javascript:NativeBridge.videoPlayer."
//                        + action + "('" + values + "');");
//            }
//        });
    }
    private void setPlayerListeners() {
        // notify player state change events
        mPlayerView
                .registerPlayerStateChange(new OnPlayerStateChangeListener() {
                    @Override
                    public boolean onStateChanged(PlayerStates state) {
                        String stateName = "";
                        switch (state) {
                        case PLAY:
                            stateName = "play";
                            break;
                        case PAUSE:
                            stateName = "pause";
                            break;
                        case END:
                            stateName = "ended";
                            break;
                        case SEEKING:
                        	stateName = "seeking";
                        	break;
                        case SEEKED:
                        	stateName = "seeked";
                        	break;
                        default:
                            break;
                        }
                        if (stateName != "") {
                            final String eventName = stateName;
                            notifyHLSPlayer("trigger", new String[] { eventName });
                        }

                        return false;
                    }
                });

        // trigger timeupdate events
//        final Runnable runUpdatePlayehead = new Runnable() {
//            @Override
//            public void run() {
//                mWebView.loadUrl("javascript:NativeBridge.videoPlayer.trigger('timeupdate', '"
//                        + mCurSec + "');");
//            }
//        };
    }
        
        
        /**
         * 
         * @param input
         *            string
         * @return given string without its first and last characters
         */
        private String getStrippedString(String input) {
            return input.substring(1, input.length() - 1);
        }
        /**
         * Notify the matching listener that event has occured
         * 
         * @param input
         *            String with event params
         * @param hashMap
         *            data provider to look the listener in
         * @param clearListeners
         *            whether to remove listeners after notifying them
         * @return true if listener was noticed, else false
         */
        private boolean notifyHLSPlayerEvent(String input,
                HashMap hashMap,
                boolean clearListeners) {
            if (hashMap != null) {
                String value = getStrippedString(input);
                // //
                // replace inner json "," delimiter so we can split with harming
                // json objects
                // value = value.replaceAll("([{][^}]+)(,)", "$1;");
                // ///
                value = value.replaceAll(("\\\\\""), "\"");
                boolean isObject = true;
                // can't split by "," since jsonString might have inner ","
                String[] params = value.split("\\{");
                // if parameter is not a json object, the delimiter is ","
                if (params.length == 1) {
                    isObject = false;
                    params = value.split(",");
                } else {
                    params[0] = params[0].substring(0, params[0].indexOf(","));
                }
                String key = getStrippedString(params[0]);
                // parse object, if sent
                Object bodyObj = null;
                if (params.length > 1 && params[1] != "null") {
                    if (isObject) { // json string
//                        String body = "{" + params[1] + "}";
//                        try {
//                            bodyObj = new JSONObject(body);
//                        } catch (JSONException e) {
//                            Log.w(TAG, "failed to parse object");
//                        }
                    } else { // simple string
                    	if ( params[1].startsWith("\"") )
                    		bodyObj = getStrippedString(params[1]);
                    	else
                    		bodyObj = params[1];
                    }
                }
                
                Object mapValue = hashMap.get(key);
                if ( mapValue instanceof HLSPlayerEventListener ) {
                    ((HLSPlayerEventListener)mapValue).onHLSPlayerEvent(bodyObj);
                } 
                else if ( mapValue instanceof ArrayList) {
                    ArrayList<HLSPlayerEventListener> listeners = (ArrayList)mapValue;
                            for (Iterator<HLSPlayerEventListener> i = listeners.iterator(); i
                                    .hasNext();) {
                                i.next().onHLSPlayerEvent(bodyObj);
                            }
                }

                if (clearListeners) {
                    hashMap.remove(key);
                }

                return true;
            }

            return false;
        }   
    
}
