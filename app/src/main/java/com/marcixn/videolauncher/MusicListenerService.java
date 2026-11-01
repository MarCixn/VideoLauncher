package com.marcixn.videolauncher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.util.List;

public class MusicListenerService extends NotificationListenerService {

    private static final String TAG = "MusicListener";

    // Static listener for direct communication with MainActivity
    public interface MusicUpdateListener {
        void onMusicUpdate(String title, String artist, String state, boolean isPlaying);
    }

    private static MusicUpdateListener listener;

    // Store last known state
    private static String lastTitle = "";
    private static String lastArtist = "";
    private static String lastState = "";
    private static boolean lastIsPlaying = false;

    public static void setMusicUpdateListener(MusicUpdateListener l) {
        listener = l;
        // Send current state immediately when listener registers
        if (l != null && !lastTitle.isEmpty()) {
            final String t = lastTitle;
            final String a = lastArtist;
            final String s = lastState;
            final boolean p = lastIsPlaying;
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (listener != null) {
                    listener.onMusicUpdate(t, a, s, p);
                }
            });
        }
    }

    private MediaSessionManager mediaSessionManager;
    private MediaSessionManager.OnActiveSessionsChangedListener sessionsListener;
    private MediaController.Callback mediaCallback;
    private MediaController activeController;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "MusicListenerService created");

        mediaSessionManager = (MediaSessionManager) getSystemService(Context.MEDIA_SESSION_SERVICE);

        mediaCallback = new MediaController.Callback() {
            @Override
            public void onMetadataChanged(MediaMetadata metadata) {
                if (metadata != null) {
                    sendMusicUpdate(metadata, null);
                }
            }

            @Override
            public void onPlaybackStateChanged(PlaybackState state) {
                if (activeController != null) {
                    sendMusicUpdate(activeController.getMetadata(), state);
                }
            }
        };

        sessionsListener = controllers -> {
            Log.d(TAG, "Active sessions changed: " + (controllers != null ? controllers.size() : 0));
            updateActiveController(controllers);
        };

        try {
            ComponentName componentName = new ComponentName(this, MusicListenerService.class);
            mediaSessionManager.addOnActiveSessionsChangedListener(sessionsListener, componentName);

            // Get initial sessions
            List<MediaController> controllers = mediaSessionManager.getActiveSessions(componentName);
            updateActiveController(controllers);
        } catch (SecurityException e) {
            Log.e(TAG, "No notification listener permission", e);
        }
    }

    private void updateActiveController(List<MediaController> controllers) {
        // Unregister from old controller
        if (activeController != null) {
            try {
                activeController.unregisterCallback(mediaCallback);
            } catch (Exception ignored) {}
        }

        activeController = null;

        if (controllers != null && !controllers.isEmpty()) {
            // Find first controller with metadata or playing state
            for (MediaController controller : controllers) {
                MediaMetadata metadata = controller.getMetadata();
                PlaybackState state = controller.getPlaybackState();

                if (metadata != null || (state != null && state.getState() == PlaybackState.STATE_PLAYING)) {
                    activeController = controller;
                    break;
                }
            }

            // If no playing controller found, use first one
            if (activeController == null) {
                activeController = controllers.get(0);
            }

            // Register callback
            activeController.registerCallback(mediaCallback);

            // Send initial update
            sendMusicUpdate(activeController.getMetadata(), activeController.getPlaybackState());

            Log.d(TAG, "Active controller: " + activeController.getPackageName());
        } else {
            // No active sessions - send empty update
            sendEmptyUpdate();
        }
    }

    private void sendMusicUpdate(MediaMetadata metadata, PlaybackState state) {
        String title = "";
        String artist = "";

        if (metadata != null) {
            title = metadata.getString(MediaMetadata.METADATA_KEY_TITLE);
            artist = metadata.getString(MediaMetadata.METADATA_KEY_ARTIST);

            if (title == null) title = metadata.getString(MediaMetadata.METADATA_KEY_DISPLAY_TITLE);
            if (artist == null) artist = metadata.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST);
            if (title == null) title = "";
            if (artist == null) artist = "";
        }

        boolean isPlaying = false;
        String stateText = "Zatrzymano";

        if (state != null) {
            int stateCode = state.getState();
            switch (stateCode) {
                case PlaybackState.STATE_PLAYING:
                    isPlaying = true;
                    stateText = "▶ Odtwarzanie";
                    break;
                case PlaybackState.STATE_PAUSED:
                    stateText = "⏸ Pauza";
                    break;
                case PlaybackState.STATE_BUFFERING:
                    stateText = "⏳ Buforowanie";
                    break;
                case PlaybackState.STATE_STOPPED:
                    stateText = "⏹ Zatrzymano";
                    break;
                default:
                    stateText = "";
            }
        }

        Log.d(TAG, "Music update: " + title + " - " + artist + " [" + stateText + "]");

        // Save last state
        lastTitle = title;
        lastArtist = artist;
        lastState = stateText;
        lastIsPlaying = isPlaying;

        // Notify listener
        if (listener != null) {
            final String fTitle = title;
            final String fArtist = artist;
            final String fState = stateText;
            final boolean fPlaying = isPlaying;

            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (listener != null) {
                    listener.onMusicUpdate(fTitle, fArtist, fState, fPlaying);
                }
            });
        }
    }

    private void sendEmptyUpdate() {
        Log.d(TAG, "Music update: empty");

        // Save last state
        lastTitle = "";
        lastArtist = "";
        lastState = "";
        lastIsPlaying = false;

        if (listener != null) {
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (listener != null) {
                    listener.onMusicUpdate("", "", "", false);
                }
            });
        }
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        // Log all notifications to see what's available
        String pkg = sbn.getPackageName();
        Log.d(TAG, "Notification from: " + pkg);

        // Check for Bluetooth/music related packages
        if (pkg.contains("bluetooth") || pkg.contains("music") || pkg.contains("phone") || pkg.contains("nwd")) {
            android.app.Notification n = sbn.getNotification();
            if (n.extras != null) {
                String title = n.extras.getString(android.app.Notification.EXTRA_TITLE);
                String text = n.extras.getString(android.app.Notification.EXTRA_TEXT);
                Log.d(TAG, "  Title: " + title + ", Text: " + text);

                // If we got music metadata from notification
                if (title != null && !title.isEmpty()) {
                    // Try to detect if it's music info
                    // This is a guess - we'll refine it based on actual notifications
                    lastTitle = title;
                    lastArtist = text != null ? text : "";
                    lastState = "▶ Bluetooth";
                    lastIsPlaying = true;

                    if (listener != null) {
                        new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                            if (listener != null) {
                                listener.onMusicUpdate(lastTitle, lastArtist, lastState, lastIsPlaying);
                            }
                        });
                    }
                }
            }
        }
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        // Not used, but required
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (activeController != null) {
            try {
                activeController.unregisterCallback(mediaCallback);
            } catch (Exception ignored) {}
        }

        if (mediaSessionManager != null && sessionsListener != null) {
            mediaSessionManager.removeOnActiveSessionsChangedListener(sessionsListener);
        }

        Log.d(TAG, "MusicListenerService destroyed");
    }
}
