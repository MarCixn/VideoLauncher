package com.marcixn.videolauncher;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebSettings;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/**
 * Fullscreen activity for displaying PC screen stream via WebView.
 */
public class ScreenStreamActivity extends Activity {

    private WebView webView;
    private FrameLayout container;
    private String streamUrl;
    private String pcIp;
    private int monitor = 3; // Default to virtual monitor
    private static final int CONTROL_PORT = 5558;
    private Handler handler = new Handler();
    private int tapCount = 0;
    private long lastTapTime = 0;
    private float lastTouchX = 0;
    private float lastTouchY = 0;

    // Pinch-to-zoom
    private ScaleGestureDetector scaleDetector;
    private float scaleFactor = 1.0f;
    private float minScale = 1.0f;
    private float maxScale = 5.0f;
    private float translateX = 0f;
    private float translateY = 0f;
    private float lastPanX = 0f;
    private float lastPanY = 0f;
    private boolean isPanning = false;
    private int activePointerId = -1;
    private long lastDoubleTapTime = 0;
    private static final long DOUBLE_TAP_TIMEOUT = 300;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Fullscreen
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );

        // Get stream URL from intent
        streamUrl = getIntent().getStringExtra("stream_url");
        if (streamUrl == null) {
            Toast.makeText(this, "No stream URL", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        // Extract PC IP from stream URL (http://IP:port)
        try {
            String urlPart = streamUrl.replace("http://", "");
            pcIp = urlPart.split(":")[0];
        } catch (Exception e) {
            pcIp = "192.168.0.189";
        }

        // Get monitor number from intent
        monitor = getIntent().getIntExtra("monitor", 3);

        // Create container for zoom/pan
        container = new FrameLayout(this);
        container.setBackgroundColor(0xFF000000);

        // Create WebView
        webView = new WebView(this);
        webView.setBackgroundColor(0xFF000000);

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(false);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        settings.setBuiltInZoomControls(false);
        settings.setDisplayZoomControls(false);
        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);

        // Handle errors and auto-reconnect
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
                // Auto-reconnect after 1 second
                handler.postDelayed(() -> {
                    if (webView != null && streamUrl != null) {
                        webView.loadUrl(streamUrl);
                    }
                }, 1000);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                // Reset reconnect timer on successful load
            }
        });

        // Periodic connection check - reload if stream stalls
        startConnectionMonitor();

        container.addView(webView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(container);

        // Initialize pinch-to-zoom detector
        scaleDetector = new ScaleGestureDetector(this, new ScaleListener());

        // Load stream URL directly (MJPEG shows as image)
        webView.loadUrl(streamUrl);
    }

    /**
     * Scale gesture listener for pinch-to-zoom.
     */
    private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            float prevScale = scaleFactor;
            scaleFactor *= detector.getScaleFactor();
            scaleFactor = Math.max(minScale, Math.min(scaleFactor, maxScale));

            // Zoom towards pinch center
            float focusX = detector.getFocusX();
            float focusY = detector.getFocusY();
            float scaleChange = scaleFactor / prevScale;

            translateX = focusX - scaleChange * (focusX - translateX);
            translateY = focusY - scaleChange * (focusY - translateY);

            constrainTranslation();
            applyTransform();
            return true;
        }
    }

    /**
     * Constrain translation to keep content on screen.
     */
    private void constrainTranslation() {
        if (webView == null) return;

        float maxTransX = (scaleFactor - 1) * webView.getWidth() / 2;
        float maxTransY = (scaleFactor - 1) * webView.getHeight() / 2;

        translateX = Math.max(-maxTransX, Math.min(maxTransX, translateX));
        translateY = Math.max(-maxTransY, Math.min(maxTransY, translateY));
    }

    /**
     * Apply scale and translation to WebView.
     */
    private void applyTransform() {
        if (webView == null) return;
        webView.setScaleX(scaleFactor);
        webView.setScaleY(scaleFactor);
        webView.setTranslationX(translateX);
        webView.setTranslationY(translateY);
    }

    @Override
    protected void onResume() {
        super.onResume();
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (webView != null) {
            webView.stopLoading();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopConnectionMonitor();
        if (webView != null) {
            webView.stopLoading();
            webView.destroy();
            webView = null;
        }
    }

    @Override
    public void onBackPressed() {
        finish();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // Pass to scale detector first
        scaleDetector.onTouchEvent(event);

        int action = event.getActionMasked();
        int pointerCount = event.getPointerCount();

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                lastTouchX = event.getX();
                lastTouchY = event.getY();
                lastPanX = event.getX();
                lastPanY = event.getY();
                activePointerId = event.getPointerId(0);
                isPanning = false;
                break;

            case MotionEvent.ACTION_MOVE:
                if (pointerCount == 1 && scaleFactor > 1.0f && !scaleDetector.isInProgress()) {
                    // Single finger pan when zoomed
                    int pointerIndex = event.findPointerIndex(activePointerId);
                    if (pointerIndex >= 0) {
                        float x = event.getX(pointerIndex);
                        float y = event.getY(pointerIndex);
                        float dx = x - lastPanX;
                        float dy = y - lastPanY;

                        if (Math.abs(dx) > 5 || Math.abs(dy) > 5) {
                            isPanning = true;
                        }

                        if (isPanning) {
                            translateX += dx;
                            translateY += dy;
                            constrainTranslation();
                            applyTransform();
                        }

                        lastPanX = x;
                        lastPanY = y;
                    }
                } else if (pointerCount > 1) {
                    isPanning = true; // Pinching counts as panning (no click)
                }
                break;

            case MotionEvent.ACTION_UP:
                if (!isPanning && !scaleDetector.isInProgress() && webView != null) {
                    float dx = Math.abs(event.getX() - lastTouchX);
                    float dy = Math.abs(event.getY() - lastTouchY);

                    // Only process as tap if finger didn't move much
                    if (dx < 30 && dy < 30) {
                        long now = System.currentTimeMillis();

                        // Check for double tap to reset zoom
                        if (now - lastDoubleTapTime < DOUBLE_TAP_TIMEOUT) {
                            resetZoom();
                            lastDoubleTapTime = 0;
                        } else {
                            // Single tap - send click to PC
                            lastDoubleTapTime = now;

                            // Convert screen touch to content position (accounting for zoom/pan)
                            float touchX = event.getX();
                            float touchY = event.getY();

                            // Adjust for zoom and pan
                            float centerX = webView.getWidth() / 2f;
                            float centerY = webView.getHeight() / 2f;

                            // Convert touch to content coordinates
                            float contentX = (touchX - centerX - translateX) / scaleFactor + centerX;
                            float contentY = (touchY - centerY - translateY) / scaleFactor + centerY;

                            // Calculate relative position
                            float relX = contentX / webView.getWidth();
                            float relY = contentY / webView.getHeight();

                            // Clamp to valid range
                            relX = Math.max(0f, Math.min(1f, relX));
                            relY = Math.max(0f, Math.min(1f, relY));

                            sendClickToPC(relX, relY);
                        }
                    }
                }
                activePointerId = -1;
                isPanning = false;
                break;

            case MotionEvent.ACTION_CANCEL:
                activePointerId = -1;
                isPanning = false;
                break;

            case MotionEvent.ACTION_POINTER_UP:
                // Handle secondary pointer up
                int pointerIndex = event.getActionIndex();
                int pointerId = event.getPointerId(pointerIndex);
                if (pointerId == activePointerId) {
                    int newPointerIndex = pointerIndex == 0 ? 1 : 0;
                    if (newPointerIndex < event.getPointerCount()) {
                        lastPanX = event.getX(newPointerIndex);
                        lastPanY = event.getY(newPointerIndex);
                        activePointerId = event.getPointerId(newPointerIndex);
                    }
                }
                break;
        }

        return true;
    }

    /**
     * Double tap to reset zoom.
     */
    private void resetZoom() {
        scaleFactor = 1.0f;
        translateX = 0f;
        translateY = 0f;
        applyTransform();
    }

    private Runnable connectionMonitor;
    private long lastReloadTime = 0;
    private static final long RELOAD_INTERVAL = 30000; // Reload every 30s to prevent stall

    /**
     * Start monitoring connection and auto-reload if needed.
     */
    private void startConnectionMonitor() {
        connectionMonitor = new Runnable() {
            @Override
            public void run() {
                if (webView != null && streamUrl != null) {
                    long now = System.currentTimeMillis();
                    // Reload stream periodically to prevent WebView timeout
                    if (now - lastReloadTime > RELOAD_INTERVAL) {
                        lastReloadTime = now;
                        webView.reload();
                    }
                }
                handler.postDelayed(this, 10000); // Check every 10s
            }
        };
        lastReloadTime = System.currentTimeMillis();
        handler.postDelayed(connectionMonitor, 10000);
    }

    /**
     * Stop connection monitor.
     */
    private void stopConnectionMonitor() {
        if (connectionMonitor != null) {
            handler.removeCallbacks(connectionMonitor);
            connectionMonitor = null;
        }
    }

    /**
     * Send click command to PC via UDP.
     * @param relX relative X position (0.0 - 1.0)
     * @param relY relative Y position (0.0 - 1.0)
     */
    private void sendClickToPC(final float relX, final float relY) {
        new Thread(() -> {
            try {
                JSONObject cmd = new JSONObject();
                cmd.put("action", "click");
                cmd.put("x", relX);
                cmd.put("y", relY);
                cmd.put("monitor", monitor);

                byte[] data = cmd.toString().getBytes();
                DatagramSocket socket = new DatagramSocket();
                socket.setSoTimeout(500);
                InetAddress address = InetAddress.getByName(pcIp);
                DatagramPacket packet = new DatagramPacket(data, data.length, address, CONTROL_PORT);
                socket.send(packet);
                socket.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }
}
