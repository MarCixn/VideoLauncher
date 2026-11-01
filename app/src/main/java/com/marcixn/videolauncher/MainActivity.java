package com.marcixn.videolauncher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.RandomAccessFile;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import android.app.ActivityManager;
import android.text.TextUtils;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import org.json.JSONObject;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private MediaPlayer mediaPlayer;
    private ImageView bluetoothButton, settingsButton, argbButton;
    private TextView clockText, swipeHint, folderIndicator, statsText;
    private LinearLayout bottomBar;
    private View fadeOverlay;
    private GestureDetector gestureDetector;
    private SharedPreferences prefs;

    // Music info display
    private LinearLayout musicInfoContainer;
    private TextView musicTitle, musicArtist, musicState;
    private boolean showMusicInfo = true;
    private boolean notificationListenerAsked = false;
    private boolean btMusicPlaying = false;


    private List<File> videoFiles = new ArrayList<>();
    private int currentVideoIndex = 0;
    private boolean isLooping = true;
    private int playDuration = 0;
    private boolean showClock = true;
    private String currentStyle = "cyberpunk";
    private Handler handler = new Handler();
    private Handler btKeepAliveHandler = new Handler();
    private Handler statsHandler = new Handler();
    private boolean surfaceReady = false;

    // Per-folder settings
    private boolean[] folderFade = new boolean[4];
    private boolean[] folderShuffle = new boolean[4];
    private boolean currentFolderFade = false;
    private boolean currentFolderShuffle = false;

    // Stats
    private boolean showAndroidStats = false;
    private boolean showPcStats = false;
    private long lastCpuTotal = 0;
    private long lastCpuIdle = 0;

    // PC Stats receiver
    private DatagramSocket pcStatsSocket;
    private Thread pcStatsThread;
    private volatile boolean pcStatsRunning = false;
    private String lastPcStats = "";
    private JSONObject lastPcStatsJson = null;
    private long lastPcStatsTime = 0;
    private boolean pcConnected = false;

    // Stats display mode
    private boolean statsUseBars = false;
    private LinearLayout statsBarsLeft;
    private View cpuBar, gpuBar, ramBar, gpuTempBar, netBar;
    private TextView cpuValue, gpuValue, ramValue, gpuTempValue, netValue;

    // Quick folder presets
    private String[] folderPresets = {
        "/sdcard/Movies",
        "/sdcard/Download/wideo",
        "/sdcard/Download",
        "/sdcard/DCIM"
    };
    private int currentFolderIndex = 0;

    private SimpleDateFormat timeFormat = new SimpleDateFormat("HH:mm", Locale.getDefault());

    private BroadcastReceiver timeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            updateClock();
        }
    };

    // Bluetooth AVRCP metadata receiver
    private BroadcastReceiver btMusicReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            android.util.Log.d("BTMusic", "Received: " + action);

            // Handle play/pause state
            if ("com.bt.ACTION_BT_MUSIC_PLAY".equals(action)) {
                btMusicPlaying = true;
                updatePlayState();
                return;
            }
            if ("com.bt.ACTION_BT_MUSIC_PAUSE".equals(action) ||
                "com.bt.ACTION_BT_MUSIC_STOP".equals(action)) {
                btMusicPlaying = false;
                updatePlayState();
                return;
            }


            // Handle music metadata
            if ("com.bt.ACTION_AVRCP_MUSIC_ID3".equals(action)) {
                String title = intent.getStringExtra("extra_avrcp_id3_title");
                String artist = intent.getStringExtra("extra_avrcp_id3_artist");

                if (title != null && !title.isEmpty()) {
                    String state = btMusicPlaying ? "▶" : "⏸";
                    updateMusicDisplay(title, artist != null ? artist : "", state, btMusicPlaying);
                }
                return;
            }

            // Fallback for local music
            String title = intent.getStringExtra("track");
            if (title == null) title = intent.getStringExtra("title");
            String artist = intent.getStringExtra("artist");

            if (title != null && !title.isEmpty()) {
                updateMusicDisplay(title, artist != null ? artist : "", "▶", true);
            }
        }
    };

    private MusicListenerService.MusicUpdateListener musicListener = (title, artist, state, isPlaying) -> {
        updateMusicDisplay(title, artist, state, isPlaying);
    };

    // Keep phone app alive every 10 seconds
    private Runnable btKeepAliveRunnable = new Runnable() {
        @Override
        public void run() {
            keepPhoneAppAlive();
            btKeepAliveHandler.postDelayed(this, 10000);
        }
    };

    // Update stats every 2 seconds
    private Runnable statsRunnable = new Runnable() {
        @Override
        public void run() {
            if (showAndroidStats || showPcStats) {
                updateStatsDisplay();
            }
            statsHandler.postDelayed(this, 2000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );

        setContentView(R.layout.activity_main);

        prefs = getSharedPreferences("VideoLauncher", MODE_PRIVATE);

        surfaceView = findViewById(R.id.surfaceView);
        bluetoothButton = findViewById(R.id.bluetoothButton);
        settingsButton = findViewById(R.id.settingsButton);
        argbButton = findViewById(R.id.argbButton);
        clockText = findViewById(R.id.clockText);
        swipeHint = findViewById(R.id.swipeHint);
        bottomBar = findViewById(R.id.bottomBar);
        folderIndicator = findViewById(R.id.folderIndicator);
        fadeOverlay = findViewById(R.id.fadeOverlay);
        statsText = findViewById(R.id.statsText);

        // Stats bars
        statsBarsLeft = findViewById(R.id.statsBarsLeft);
        cpuBar = findViewById(R.id.cpuBar);
        gpuBar = findViewById(R.id.gpuBar);
        ramBar = findViewById(R.id.ramBar);
        gpuTempBar = findViewById(R.id.gpuTempBar);
        netBar = findViewById(R.id.netBar);
        cpuValue = findViewById(R.id.cpuValue);
        gpuValue = findViewById(R.id.gpuValue);
        ramValue = findViewById(R.id.ramValue);
        gpuTempValue = findViewById(R.id.gpuTempValue);
        netValue = findViewById(R.id.netValue);

        // Music info
        musicInfoContainer = findViewById(R.id.musicInfoContainer);
        musicTitle = findViewById(R.id.musicTitle);
        musicArtist = findViewById(R.id.musicArtist);
        musicState = findViewById(R.id.musicState);


        // Setup SurfaceView
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

        loadFolderPresets();
        setupGestures();
        setupButtons();
        loadSettings();
        applyStyle();
        loadVideos();
        updateClock();
        updateFolderIndicator();

        IntentFilter filter = new IntentFilter(Intent.ACTION_TIME_TICK);
        registerReceiver(timeReceiver, filter);

        // Register music listener
        MusicListenerService.setMusicUpdateListener(musicListener);

        // Register Bluetooth AVRCP metadata receiver - specific to this radio
        IntentFilter btFilter = new IntentFilter();
        btFilter.addAction("com.bt.ACTION_AVRCP_MUSIC_ID3");  // Music metadata
        btFilter.addAction("com.bt.ACTION_BT_MUSIC_PLAY");    // Play state
        btFilter.addAction("com.bt.ACTION_BT_MUSIC_PAUSE");   // Pause state
        btFilter.addAction("com.bt.ACTION_BT_MUSIC_STOP");    // Stop state
        btFilter.addAction("com.android.music.metachanged");
        btFilter.addAction("com.android.music.playstatechanged");
        registerReceiver(btMusicReceiver, btFilter);

        // Start keep-alive service for phone app
        startService(new Intent(this, KeepAliveService.class));
        keepPhoneAppAlive();
        btKeepAliveHandler.postDelayed(btKeepAliveRunnable, 10000);
    }

    // SurfaceHolder.Callback implementation
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceReady = true;
        playCurrentVideo();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // Not needed
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceReady = false;
        releaseMediaPlayer();
    }

    private void releaseMediaPlayer() {
        if (mediaPlayer != null) {
            try {
                mediaPlayer.stop();
            } catch (Exception ignored) {}
            try {
                mediaPlayer.release();
            } catch (Exception ignored) {}
            mediaPlayer = null;
        }
    }

    private void loadFolderPresets() {
        String preset1 = prefs.getString("folderPreset1", "/sdcard/Movies");
        String preset2 = prefs.getString("folderPreset2", "/sdcard/Download/wideo");
        String preset3 = prefs.getString("folderPreset3", "/sdcard/Download");
        String preset4 = prefs.getString("folderPreset4", "/sdcard/DCIM");

        folderPresets = new String[]{preset1, preset2, preset3, preset4};

        // Load per-folder settings
        folderFade[0] = prefs.getBoolean("folder1Fade", false);
        folderFade[1] = prefs.getBoolean("folder2Fade", false);
        folderFade[2] = prefs.getBoolean("folder3Fade", false);
        folderFade[3] = prefs.getBoolean("folder4Fade", false);
        folderShuffle[0] = prefs.getBoolean("folder1Shuffle", false);
        folderShuffle[1] = prefs.getBoolean("folder2Shuffle", false);
        folderShuffle[2] = prefs.getBoolean("folder3Shuffle", false);
        folderShuffle[3] = prefs.getBoolean("folder4Shuffle", false);

        String currentFolder = prefs.getString("videoFolder", "/sdcard/Movies");
        for (int i = 0; i < folderPresets.length; i++) {
            if (folderPresets[i].equals(currentFolder)) {
                currentFolderIndex = i;
                break;
            }
        }

        // Apply current folder settings
        currentFolderFade = folderFade[currentFolderIndex];
        currentFolderShuffle = folderShuffle[currentFolderIndex];
    }

    private void keepPhoneAppAlive() {
        try {
            Intent btIntent = new Intent("com.nwd.btbroad.service.ACTION_START");
            btIntent.setPackage("com.nwd.android.phone");
            sendBroadcast(btIntent);

            Intent serviceIntent = new Intent();
            serviceIntent.setComponent(new ComponentName(
                "com.nwd.android.phone",
                "com.visualframe.btphone.BtBroadcastReceiver"
            ));
            serviceIntent.setAction("com.nwd.btbroad.service.ACTION_START");
            sendBroadcast(serviceIntent);

        } catch (Exception e) {
            // Ignore
        }
    }

    private void setupGestures() {
        gestureDetector = new GestureDetector(this, new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
                if (e1 == null || e2 == null) return false;

                float diffY = e2.getY() - e1.getY();
                float diffX = e2.getX() - e1.getX();

                if (diffY < -100 && Math.abs(diffY) > Math.abs(diffX)) {
                    openAppDrawer();
                    return true;
                }

                if (diffX < -100 && Math.abs(diffX) > Math.abs(diffY)) {
                    switchToNextFolder();
                    return true;
                }

                if (diffX > 100 && Math.abs(diffX) > Math.abs(diffY)) {
                    switchToPreviousFolder();
                    return true;
                }

                return false;
            }

            @Override
            public boolean onDown(MotionEvent e) {
                return true;
            }
        });

        View rootView = findViewById(R.id.rootLayout);
        rootView.setOnTouchListener((v, event) -> {
            gestureDetector.onTouchEvent(event);
            return true;
        });
    }

    private void switchToNextFolder() {
        currentFolderIndex = (currentFolderIndex + 1) % folderPresets.length;
        applyFolderChange();
    }

    private void switchToPreviousFolder() {
        currentFolderIndex = (currentFolderIndex - 1 + folderPresets.length) % folderPresets.length;
        applyFolderChange();
    }

    private void applyFolderChange() {
        String newFolder = folderPresets[currentFolderIndex];

        File folder = new File(newFolder);
        if (!folder.exists() || !folder.isDirectory()) {
            Toast.makeText(this, "Folder nie istnieje: " + newFolder, Toast.LENGTH_SHORT).show();
            return;
        }

        prefs.edit().putString("videoFolder", newFolder).apply();

        // Update current folder settings
        currentFolderFade = folderFade[currentFolderIndex];
        currentFolderShuffle = folderShuffle[currentFolderIndex];

        currentVideoIndex = 0;
        loadVideos();

        if (videoFiles.isEmpty()) {
            Toast.makeText(this, "Brak wideo w: " + newFolder, Toast.LENGTH_SHORT).show();
        } else {
            String info = getFolderName(newFolder) + " (" + videoFiles.size() + ")";
            if (currentFolderShuffle) info += " [Shuffle]";
            if (currentFolderFade) info += " [Fade]";
            Toast.makeText(this, info, Toast.LENGTH_SHORT).show();
            playCurrentVideo();
        }

        updateFolderIndicator();
    }

    private String getFolderName(String path) {
        if (path.contains("/")) {
            return path.substring(path.lastIndexOf("/") + 1);
        }
        return path;
    }

    private void updateFolderIndicator() {
        if (folderIndicator != null) {
            String currentFolder = folderPresets[currentFolderIndex];
            folderIndicator.setText(getFolderName(currentFolder));
        }
    }

    private void setupButtons() {
        bluetoothButton.setOnClickListener(v -> {
            Intent intent = getPackageManager().getLaunchIntentForPackage("com.nwd.android.phone");
            if (intent != null) {
                startActivity(intent);
            } else {
                intent = new Intent();
                intent.setClassName("com.nwd.android.phone", "com.nwd.android.phone.home_horizontalActivity");
                try {
                    startActivity(intent);
                } catch (Exception e) {
                    startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));
                }
            }
        });

        settingsButton.setOnClickListener(v -> {
            Intent intent = new Intent(this, SettingsActivity.class);
            startActivity(intent);
        });

        if (argbButton != null) {
            argbButton.setOnClickListener(v -> {
                Intent intent = new Intent(this, ARGBControlActivity.class);
                startActivity(intent);
            });
        }
    }

    private void loadSettings() {
        isLooping = prefs.getBoolean("loopMode", true);
        playDuration = prefs.getInt("playDuration", 0);
        showClock = prefs.getBoolean("showClock", true);
        currentStyle = prefs.getString("style", "cyberpunk");
        showAndroidStats = prefs.getBoolean("showAndroidStats", false);
        showPcStats = prefs.getBoolean("showPcStats", false);
        statsUseBars = prefs.getBoolean("statsUseBars", false);
        showMusicInfo = prefs.getBoolean("showMusicInfo", true);

        if (clockText != null) {
            clockText.setVisibility(showClock ? View.VISIBLE : View.GONE);
        }

        boolean showStats = showAndroidStats || showPcStats;

        if (statsText != null) {
            statsText.setVisibility(showStats && !statsUseBars ? View.VISIBLE : View.GONE);
        }

        if (statsBarsLeft != null) {
            statsBarsLeft.setVisibility(showStats && statsUseBars ? View.VISIBLE : View.GONE);
        }

        // Music info visibility is handled by updateMusicDisplay based on content
    }

    private void applyStyle() {
        int accentColor;
        int barColor;

        switch (currentStyle) {
            case "neon":
                accentColor = Color.parseColor("#FF00FF");
                barColor = Color.parseColor("#80330033");
                break;
            case "minimal":
                accentColor = Color.parseColor("#FFFFFF");
                barColor = Color.parseColor("#60000000");
                break;
            case "red":
                accentColor = Color.parseColor("#FF0040");
                barColor = Color.parseColor("#80330000");
                break;
            case "cyberpunk":
            default:
                accentColor = Color.parseColor("#00FFFF");
                barColor = Color.parseColor("#80000000");
                break;
        }

        if (clockText != null) {
            clockText.setTextColor(accentColor);
        }

        if (bluetoothButton != null) {
            bluetoothButton.setColorFilter(accentColor, PorterDuff.Mode.SRC_IN);
        }
        if (settingsButton != null) {
            settingsButton.setColorFilter(accentColor, PorterDuff.Mode.SRC_IN);
        }

        if (argbButton != null) {
            // ARGB button keeps its RGB colors, just adjust alpha
            argbButton.setAlpha(0.9f);
        }

        if (bottomBar != null) {
            bottomBar.setBackgroundColor(barColor);
        }

        if (swipeHint != null) {
            swipeHint.setTextColor((accentColor & 0x00FFFFFF) | 0x60000000);
        }

        if (folderIndicator != null) {
            folderIndicator.setTextColor((accentColor & 0x00FFFFFF) | 0xB0000000);
        }
    }

    private void updateClock() {
        if (clockText != null && showClock) {
            clockText.setText(timeFormat.format(new Date()));
        }
    }

    private void loadVideos() {
        videoFiles.clear();
        String videoFolder = prefs.getString("videoFolder", "/sdcard/Movies");
        File folder = new File(videoFolder);

        if (folder.exists() && folder.isDirectory()) {
            File[] files = folder.listFiles((dir, name) -> {
                String lower = name.toLowerCase();
                return lower.endsWith(".mp4") || lower.endsWith(".mkv") ||
                       lower.endsWith(".avi") || lower.endsWith(".mov");
            });

            if (files != null && files.length > 0) {
                videoFiles.addAll(Arrays.asList(files));

                // Shuffle if enabled for this folder
                if (currentFolderShuffle) {
                    Collections.shuffle(videoFiles, new Random());
                }
            }
        }

        if (videoFiles.isEmpty()) {
            File defaultVideo = new File("/sdcard/Download/cyberpunk_loop.mp4");
            if (defaultVideo.exists()) {
                videoFiles.add(defaultVideo);
            }
        }
    }

    private void playCurrentVideo() {
        if (videoFiles.isEmpty() || !surfaceReady) {
            return;
        }

        handler.removeCallbacksAndMessages(null);
        releaseMediaPlayer();

        try {
            File currentVideo = videoFiles.get(currentVideoIndex);

            mediaPlayer = new MediaPlayer();

            // NO AUDIO SETUP - just video!
            // Don't call setAudioStreamType or any audio-related methods

            mediaPlayer.setDataSource(currentVideo.getAbsolutePath());
            mediaPlayer.setSurface(surfaceHolder.getSurface());

            mediaPlayer.setOnPreparedListener(mp -> {
                // Mute just in case (but audio shouldn't even initialize)
                try {
                    mp.setVolume(0f, 0f);
                } catch (Exception ignored) {}

                mp.start();

                if (playDuration > 0 && !isLooping) {
                    handler.postDelayed(this::playNextVideo, playDuration * 1000);
                }
            });

            mediaPlayer.setOnCompletionListener(mp -> {
                if (isLooping && videoFiles.size() == 1) {
                    // Loop same video
                    mp.seekTo(0);
                    mp.start();
                } else {
                    // For fade: seek back slightly and pause to keep last frame visible
                    if (currentFolderFade) {
                        try {
                            int duration = mp.getDuration();
                            mp.seekTo(Math.max(0, duration - 100));
                            mp.pause();
                        } catch (Exception ignored) {}
                    }
                    playNextVideo();
                }
            });

            mediaPlayer.setOnErrorListener((mp, what, extra) -> {
                // On error, try next video
                handler.postDelayed(this::playNextVideo, 1000);
                return true;
            });

            mediaPlayer.prepareAsync();

        } catch (Exception e) {
            // On error, try next video
            handler.postDelayed(this::playNextVideo, 1000);
        }
    }

    private void playNextVideo() {
        handler.removeCallbacksAndMessages(null);

        if (isLooping && videoFiles.size() == 1) {
            // Single video loop - just seek to start
            if (mediaPlayer != null) {
                try {
                    mediaPlayer.seekTo(0);
                    mediaPlayer.start();
                } catch (Exception e) {
                    playCurrentVideo();
                }
            }
        } else {
            // Next video in sequence
            currentVideoIndex = (currentVideoIndex + 1) % videoFiles.size();

            if (currentFolderFade && fadeOverlay != null) {
                // Fade to black, then play next video
                fadeToBlackAndPlay();
            } else {
                playCurrentVideo();
            }
        }
    }

    private void fadeToBlackAndPlay() {
        if (fadeOverlay == null) {
            playCurrentVideo();
            return;
        }

        fadeOverlay.setVisibility(View.VISIBLE);
        AlphaAnimation fadeIn = new AlphaAnimation(0f, 1f);
        fadeIn.setDuration(300);
        fadeIn.setFillAfter(true);
        fadeIn.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) {}

            @Override
            public void onAnimationEnd(Animation animation) {
                // FIRST stop old video so we see black, not last frame
                releaseMediaPlayer();

                // Short delay to ensure surface is cleared
                handler.postDelayed(() -> {
                    playCurrentVideoWithFadeOut();
                }, 50);
            }

            @Override
            public void onAnimationRepeat(Animation animation) {}
        });
        fadeOverlay.startAnimation(fadeIn);
    }

    private void playCurrentVideoWithFadeOut() {
        if (videoFiles.isEmpty() || !surfaceReady) {
            fadeOutOverlay();
            return;
        }

        handler.removeCallbacksAndMessages(null);
        releaseMediaPlayer();

        try {
            File currentVideo = videoFiles.get(currentVideoIndex);

            mediaPlayer = new MediaPlayer();
            mediaPlayer.setDataSource(currentVideo.getAbsolutePath());
            mediaPlayer.setSurface(surfaceHolder.getSurface());

            mediaPlayer.setOnPreparedListener(mp -> {
                try {
                    mp.setVolume(0f, 0f);
                } catch (Exception ignored) {}

                mp.start();
                fadeOutOverlay();

                if (playDuration > 0 && !isLooping) {
                    handler.postDelayed(this::playNextVideo, playDuration * 1000);
                }
            });

            mediaPlayer.setOnCompletionListener(mp -> {
                if (isLooping && videoFiles.size() == 1) {
                    mp.seekTo(0);
                    mp.start();
                } else {
                    playNextVideo();
                }
            });

            mediaPlayer.setOnErrorListener((mp, what, extra) -> {
                fadeOutOverlay();
                handler.postDelayed(this::playNextVideo, 1000);
                return true;
            });

            mediaPlayer.prepareAsync();

        } catch (Exception e) {
            fadeOutOverlay();
            handler.postDelayed(this::playNextVideo, 1000);
        }
    }

    private void fadeOutOverlay() {
        if (fadeOverlay == null) return;

        AlphaAnimation fadeOut = new AlphaAnimation(1f, 0f);
        fadeOut.setDuration(300);
        fadeOut.setFillAfter(true);
        fadeOut.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) {}

            @Override
            public void onAnimationEnd(Animation animation) {
                fadeOverlay.setVisibility(View.GONE);
            }

            @Override
            public void onAnimationRepeat(Animation animation) {}
        });
        fadeOverlay.startAnimation(fadeOut);
    }

    private void openAppDrawer() {
        Intent intent = new Intent(this, AppDrawerActivity.class);
        startActivity(intent);
        overridePendingTransition(R.anim.slide_up_in, R.anim.fade_out);
    }

    @Override
    protected void onResume() {
        super.onResume();
        loadFolderPresets();
        loadSettings();
        applyStyle();
        loadVideos();
        updateClock();
        updateFolderIndicator();

        if (surfaceReady && (mediaPlayer == null || !mediaPlayer.isPlaying())) {
            playCurrentVideo();
        }

        keepPhoneAppAlive();

        // Start stats updates if enabled
        if (showAndroidStats || showPcStats) {
            statsHandler.removeCallbacksAndMessages(null);
            statsHandler.post(statsRunnable);
        }

        // Start PC stats receiver if enabled
        if (showPcStats) {
            startPcStatsReceiver();
        }

        // Check notification listener permission for music info
        checkNotificationListenerPermission();

        // Re-register music listener
        MusicListenerService.setMusicUpdateListener(musicListener);

        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    @Override
    protected void onPause() {
        super.onPause();
        handler.removeCallbacksAndMessages(null);
        statsHandler.removeCallbacksAndMessages(null);
        stopPcStatsReceiver();
        if (mediaPlayer != null) {
            try {
                mediaPlayer.pause();
            } catch (Exception ignored) {}
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        btKeepAliveHandler.removeCallbacksAndMessages(null);
        handler.removeCallbacksAndMessages(null);
        statsHandler.removeCallbacksAndMessages(null);
        stopPcStatsReceiver();
        releaseMediaPlayer();
        try {
            unregisterReceiver(timeReceiver);
        } catch (Exception ignored) {}
        try {
            unregisterReceiver(btMusicReceiver);
        } catch (Exception ignored) {}
        // Clear music listener
        MusicListenerService.setMusicUpdateListener(null);
    }

    @Override
    public void onBackPressed() {
        // Do nothing - we're the launcher
    }

    private float getCpuUsage() {
        try {
            RandomAccessFile reader = new RandomAccessFile("/proc/stat", "r");
            String load = reader.readLine();
            reader.close();

            String[] tokens = load.split("\\s+");

            long idle = Long.parseLong(tokens[4]);
            long total = 0;
            for (int i = 1; i < tokens.length; i++) {
                total += Long.parseLong(tokens[i]);
            }

            if (lastCpuTotal == 0 || lastCpuIdle == 0) {
                lastCpuTotal = total;
                lastCpuIdle = idle;
                return 0;
            }

            long diffTotal = total - lastCpuTotal;
            long diffIdle = idle - lastCpuIdle;

            lastCpuTotal = total;
            lastCpuIdle = idle;

            if (diffTotal == 0) return 0;

            return (float) (diffTotal - diffIdle) * 100 / diffTotal;

        } catch (Exception e) {
            return 0;
        }
    }

    private void startPcStatsReceiver() {
        if (pcStatsRunning) return;

        pcStatsRunning = true;
        pcStatsThread = new Thread(() -> {
            try {
                pcStatsSocket = new DatagramSocket(5556);
                pcStatsSocket.setSoTimeout(5000);

                byte[] buffer = new byte[1024];

                while (pcStatsRunning) {
                    try {
                        DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                        pcStatsSocket.receive(packet);

                        String data = new String(packet.getData(), 0, packet.getLength());
                        parsePcStats(data);

                    } catch (java.net.SocketTimeoutException e) {
                        // Timeout - no data received, PC might be offline
                        if (pcStatsRunning) {
                            lastPcStats = "PC: offline";
                            runOnUiThread(this::updateStatsDisplay);
                        }
                    }
                }
            } catch (Exception e) {
                lastPcStats = "PC: error";
            } finally {
                if (pcStatsSocket != null) {
                    pcStatsSocket.close();
                }
            }
        });
        pcStatsThread.start();
    }

    private void stopPcStatsReceiver() {
        pcStatsRunning = false;
        if (pcStatsSocket != null) {
            pcStatsSocket.close();
        }
        if (pcStatsThread != null) {
            pcStatsThread.interrupt();
        }
    }

    private void parsePcStats(String data) {
        try {
            // Debug: log raw data
            android.util.Log.d("PCSTATS", "Raw: " + data);

            JSONObject json = new JSONObject(data);
            lastPcStatsJson = json;
            lastPcStatsTime = System.currentTimeMillis();
            pcConnected = true;

            // Debug: log parsed values
            android.util.Log.d("PCSTATS", "GPU: " + (json.has("gpu") ? json.getDouble("gpu") : "NONE"));
            android.util.Log.d("PCSTATS", "GPU_TEMP: " + (json.has("gpu_temp") ? json.getDouble("gpu_temp") : "NONE"));

            StringBuilder sb = new StringBuilder("PC: ");

            if (json.has("cpu")) {
                sb.append(String.format(Locale.US, "CPU:%.0f%%", json.getDouble("cpu")));
            }
            if (json.has("gpu")) {
                sb.append(String.format(Locale.US, " GPU:%.0f%%", json.getDouble("gpu")));
            }
            if (json.has("ram")) {
                sb.append(String.format(Locale.US, " RAM:%.0f%%", json.getDouble("ram")));
            }
            if (json.has("cpu_temp")) {
                sb.append(String.format(Locale.US, " T:%.0f°", json.getDouble("cpu_temp")));
            } else if (json.has("gpu_temp")) {
                sb.append(String.format(Locale.US, " T:%.0f°", json.getDouble("gpu_temp")));
            }

            lastPcStats = sb.toString();
            runOnUiThread(this::updateStatsDisplay);

        } catch (Exception e) {
            lastPcStats = "PC: parse error";
            lastPcStatsJson = null;
        }
    }

    private void updateStatsDisplay() {
        if (statsUseBars) {
            updateStatsBars();
        } else {
            updateStatsText();
        }
    }

    private void updateStatsText() {
        if (statsText == null) return;

        StringBuilder display = new StringBuilder();

        // Android stats (with temperature)
        if (showAndroidStats) {
            try {
                float cpuUsage = getCpuUsage();
                ActivityManager am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
                ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
                am.getMemoryInfo(memInfo);

                long totalMem = memInfo.totalMem / (1024 * 1024);
                long availMem = memInfo.availMem / (1024 * 1024);
                long usedMem = totalMem - availMem;
                int ramPercent = (int) ((usedMem * 100) / totalMem);

                display.append(String.format(Locale.US, "Android: CPU:%.0f%% RAM:%d%%", cpuUsage, ramPercent));

                // Try to get Android CPU temperature
                Float androidTemp = getAndroidCpuTemp();
                if (androidTemp != null) {
                    display.append(String.format(Locale.US, " T:%.0f°", androidTemp));
                }
            } catch (Exception e) {
                display.append("Android: error");
            }
        }

        // PC stats
        if (showPcStats && !lastPcStats.isEmpty()) {
            if (display.length() > 0) {
                display.append("\n");
            }
            display.append(lastPcStats);
        }

        statsText.setText(display.toString());
    }

    private Float getAndroidCpuTemp() {
        // Try reading from thermal zone files
        String[] thermalPaths = {
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/thermal/thermal_zone1/temp",
            "/sys/devices/virtual/thermal/thermal_zone0/temp",
            "/sys/devices/system/cpu/cpu0/cpufreq/cpu_temp",
            "/sys/devices/platform/s5p-tmu/temperature"
        };

        for (String path : thermalPaths) {
            try {
                BufferedReader reader = new BufferedReader(new FileReader(path));
                String line = reader.readLine();
                reader.close();

                if (line != null) {
                    float temp = Float.parseFloat(line.trim());
                    // Temperature might be in millidegrees
                    if (temp > 1000) {
                        temp = temp / 1000;
                    }
                    if (temp > 0 && temp < 150) {
                        return temp;
                    }
                }
            } catch (Exception e) {
                // Try next path
            }
        }
        return null;
    }

    private void updateStatsBars() {
        if (statsBarsLeft == null) return;

        // Check if PC connection lost (no data for 5 seconds)
        long now = System.currentTimeMillis();
        if (lastPcStatsTime > 0 && now - lastPcStatsTime > 5000) {
            pcConnected = false;
        }

        if (!pcConnected || lastPcStatsJson == null) {
            // Show "no connection" state - empty bars with "--"
            setBarWidth(cpuBar, 0);
            setBarWidth(gpuBar, 0);
            setBarWidth(ramBar, 0);
            setBarWidth(gpuTempBar, 0);
            setBarWidth(netBar, 0);
            cpuValue.setText("--");
            gpuValue.setText("--");
            ramValue.setText("--");
            gpuTempValue.setText("--");
            netValue.setText("--");
            return;
        }

        try {
            // CPU
            if (lastPcStatsJson.has("cpu")) {
                int cpu = (int) lastPcStatsJson.getDouble("cpu");
                setBarWidth(cpuBar, cpu);
                cpuValue.setText(cpu + "%");
            }

            // GPU
            if (lastPcStatsJson.has("gpu")) {
                int gpu = (int) lastPcStatsJson.getDouble("gpu");
                setBarWidth(gpuBar, gpu);
                gpuValue.setText(gpu + "%");
            }

            // RAM
            if (lastPcStatsJson.has("ram")) {
                int ram = (int) lastPcStatsJson.getDouble("ram");
                setBarWidth(ramBar, ram);
                ramValue.setText(ram + "%");
            }

            // GPU Temp (scale 0-100°C)
            if (lastPcStatsJson.has("gpu_temp")) {
                int temp = (int) lastPcStatsJson.getDouble("gpu_temp");
                setBarWidth(gpuTempBar, Math.min(temp, 100));
                gpuTempValue.setText(temp + "°");
            }

            // Network (scale 0-100 MB/s)
            if (lastPcStatsJson.has("net")) {
                double net = lastPcStatsJson.getDouble("net");
                int netPercent = (int) Math.min(net, 100);
                setBarWidth(netBar, netPercent);
                if (net >= 1.0) {
                    netValue.setText(String.format(Locale.US, "%.0f MB", net));
                } else {
                    netValue.setText(String.format(Locale.US, "%.0f KB", net * 1024));
                }
            }

        } catch (Exception e) {
            // Ignore errors
        }
    }

    private void setBarWidth(View bar, int percent) {
        if (bar == null || bar.getParent() == null) return;

        View parent = (View) bar.getParent();
        int parentWidth = parent.getWidth();
        if (parentWidth == 0) return;

        int barWidth = (parentWidth * Math.max(0, Math.min(100, percent))) / 100;

        android.view.ViewGroup.LayoutParams params = bar.getLayoutParams();
        params.width = barWidth;
        bar.setLayoutParams(params);
    }

    private boolean isNotificationListenerEnabled() {
        String pkgName = getPackageName();
        String flat = Settings.Secure.getString(getContentResolver(), "enabled_notification_listeners");
        if (!TextUtils.isEmpty(flat)) {
            String[] names = flat.split(":");
            for (String name : names) {
                ComponentName cn = ComponentName.unflattenFromString(name);
                if (cn != null && TextUtils.equals(pkgName, cn.getPackageName())) {
                    return true;
                }
            }
        }
        return false;
    }

    private void updatePlayState() {
        if (musicState != null && musicInfoContainer.getVisibility() == View.VISIBLE) {
            String state = btMusicPlaying ? "▶" : "⏸";
            musicState.setText(state);

            int stateColor;
            switch (currentStyle) {
                case "neon":
                    stateColor = btMusicPlaying ? Color.parseColor("#FF00FF") : Color.parseColor("#808080");
                    break;
                case "red":
                    stateColor = btMusicPlaying ? Color.parseColor("#FF0040") : Color.parseColor("#808080");
                    break;
                case "minimal":
                    stateColor = btMusicPlaying ? Color.parseColor("#FFFFFF") : Color.parseColor("#808080");
                    break;
                default:
                    stateColor = btMusicPlaying ? Color.parseColor("#00FFFF") : Color.parseColor("#808080");
                    break;
            }
            musicState.setTextColor(stateColor);
        }
    }

    private void checkNotificationListenerPermission() {
        if (!showMusicInfo || notificationListenerAsked) {
            return;
        }

        if (!isNotificationListenerEnabled()) {
            notificationListenerAsked = true;

            new AlertDialog.Builder(this)
                .setTitle("Dostęp do powiadomień")
                .setMessage("Aby wyświetlać info o muzyce, włącz dostęp do powiadomień dla Video Launcher")
                .setPositiveButton("Otwórz ustawienia", (dialog, which) -> {
                    Intent intent = new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS");
                    try {
                        startActivity(intent);
                    } catch (Exception e) {
                        Toast.makeText(this, "Nie można otworzyć ustawień", Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("Anuluj", null)
                .show();
        }
    }

    private void updateMusicDisplay(String title, String artist, String state, boolean isPlaying) {
        if (musicInfoContainer == null) return;

        // Hide if disabled in settings
        if (!showMusicInfo) {
            musicInfoContainer.setVisibility(View.GONE);
            return;
        }

        // Hide if no music info available
        if (title == null || title.isEmpty()) {
            musicInfoContainer.setVisibility(View.GONE);
            return;
        }

        // Show music info
        musicInfoContainer.setVisibility(View.VISIBLE);

        if (musicTitle != null) {
            musicTitle.setText(title);
            // Enable marquee scrolling for long titles
            musicTitle.setSelected(true);
        }

        if (musicArtist != null) {
            if (artist != null && !artist.isEmpty()) {
                musicArtist.setText(artist);
                musicArtist.setVisibility(View.VISIBLE);
            } else {
                musicArtist.setVisibility(View.GONE);
            }
        }

        if (musicState != null) {
            if (state != null && !state.isEmpty()) {
                musicState.setText(state);
                musicState.setVisibility(View.VISIBLE);

                // Change color based on state
                int stateColor;
                switch (currentStyle) {
                    case "neon":
                        stateColor = isPlaying ? Color.parseColor("#FF00FF") : Color.parseColor("#808080");
                        break;
                    case "red":
                        stateColor = isPlaying ? Color.parseColor("#FF0040") : Color.parseColor("#808080");
                        break;
                    case "minimal":
                        stateColor = isPlaying ? Color.parseColor("#FFFFFF") : Color.parseColor("#808080");
                        break;
                    case "cyberpunk":
                    default:
                        stateColor = isPlaying ? Color.parseColor("#00FFFF") : Color.parseColor("#808080");
                        break;
                }
                musicState.setTextColor(stateColor);
            } else {
                musicState.setVisibility(View.GONE);
            }
        }
    }
}
