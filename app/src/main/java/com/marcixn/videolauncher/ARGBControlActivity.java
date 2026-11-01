package com.marcixn.videolauncher;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.util.TypedValue;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ARGBControlActivity extends Activity {

    private static final String TAG = "ARGB";
    private static final String ACTION_USB_PERMISSION = "com.marcixn.videolauncher.USB_PERMISSION";

    private UsbManager usbManager;
    private UsbDevice usbDevice;
    private UsbDeviceConnection connection;
    private ARGBSerialDevice serialDevice;

    private Handler handler = new Handler();
    private ExecutorService executor = Executors.newSingleThreadExecutor();
    private Queue<String> commandQueue = new LinkedList<>();
    private boolean isSending = false;
    private boolean isConnected = false;
    private boolean isConnecting = false;
    private Runnable reconnectRunnable;

    private Button audioBtn, connectBtn;
    private TextView statusText;

    private LinearLayout extEffectsContainer, intEffectsContainer, fanEffectsContainer;
    private SeekBar extBrightness, extSpeed;
    private LinearLayout extColorsContainer;
    private int extEffect = 1, extVariant = 0, extBrightnessVal = 200, extSpeedVal = 5;
    private Button[] extEffectBtns = new Button[13];

    private SeekBar intBrightness, intSpeed;
    private LinearLayout intColorsContainer;
    private int intEffect = 1, intVariant = 0, intBrightnessVal = 200, intSpeedVal = 5;
    private Button[] intEffectBtns = new Button[13];

    private SeekBar fanBrightness, fanSpeed;
    private LinearLayout fanColorsContainer;
    private int fanEffect = 1, fanVariant = 0, fanBrightnessVal = 200, fanSpeedVal = 5;
    private Button[] fanEffectBtns = new Button[13];

    private Button fanSilent, fanNormal, fanMax, fanManual;
    private LinearLayout fanManualContainer;
    private SeekBar fanSpeedSeekBar;
    private TextView fanSpeedText;
    private static final int FAN_SILENT = 1;
    private static final int FAN_NORMAL = 2;
    private static final int FAN_MAX = 3;
    private static final int FAN_MANUAL = 5;
    private int currentFanMode = FAN_NORMAL;
    private int currentFanPwmSpeed = 50;

    private boolean audioStateA = true;

    // Nazwy efektow (13 programow z STM32)
    private static final String[] EFFECT_NAMES = {
        "OFF", "TECZA", "PULS", "SOLID", "FALA", "ODDECH",
        "GRAD", "3KOLOR", "TEATR", "SKANER", "PLAZMA", "GONI", "WYPELN"
    };

    // Kolory dla wariantow (13 wariantow jak w STM32)
    // 0=TECZA, 1=BIALY, 2=CZERW, 3=ZIEL, 4=NIEB, 5=ROZ, 6=FIOL
    // 7=CYAN, 8=POMAR, 9=CIEPŁY, 10=ZIMNY, 11=NATURA, 12=SYNTH
    private static final int[] VARIANT_COLORS = {
        0xFFFF00FF, // 0: DEFAULT/TECZA (gradient)
        0xFFFFFFFF, // 1: WHITE
        0xFFFF0000, // 2: RED
        0xFF00FF00, // 3: GREEN
        0xFF0000FF, // 4: BLUE
        0xFFFF3296, // 5: PINK (255,50,150)
        0xFF9600FF, // 6: PURPLE (150,0,255)
        0xFF00FFFF, // 7: CYAN
        0xFFFF6400, // 8: ORANGE (255,100,0)
        0xFFFF5000, // 9: WARM (255,80,0)
        0xFF6464FF, // 10: COLD (100,100,255)
        0xFF32FF64, // 11: NATURE (50,255,100)
        0xFFFF00C8  // 12: SYNTHWAVE (255,0,200)
    };
    // Wszystkie 13 wariantow
    private static final int[] COLOR_VARIANTS = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    // Kolory paneli
    private static final int COLOR_EXT = 0xFF00BCD4;
    private static final int COLOR_INT = 0xFFE040FB;
    private static final int COLOR_FAN = 0xFFFF9800;
    private static final int COLOR_INACTIVE = 0xFF333333;

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false) && device != null) {
                    connectToDevice(device);
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                scanForDevice();
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                disconnect();
                scheduleReconnect();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_argb_control);
        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        initViews();
        setupEffectButtons();
        setupListeners();
        setupColorButtons();
        registerUsbReceiver();
        handler.postDelayed(this::scanForDevice, 500);
    }

    private void initViews() {
        audioBtn = findViewById(R.id.audioBtn);
        connectBtn = findViewById(R.id.connectBtn);
        statusText = findViewById(R.id.statusText);

        extEffectsContainer = findViewById(R.id.extEffectsContainer);
        extBrightness = findViewById(R.id.extBrightness);
        extSpeed = findViewById(R.id.extSpeed);
        extColorsContainer = findViewById(R.id.extColorsContainer);

        intEffectsContainer = findViewById(R.id.intEffectsContainer);
        intBrightness = findViewById(R.id.intBrightness);
        intSpeed = findViewById(R.id.intSpeed);
        intColorsContainer = findViewById(R.id.intColorsContainer);

        fanEffectsContainer = findViewById(R.id.fanEffectsContainer);
        fanBrightness = findViewById(R.id.fanBrightness);
        fanSpeed = findViewById(R.id.fanSpeed);
        fanColorsContainer = findViewById(R.id.fanColorsContainer);

        fanSilent = findViewById(R.id.fanSilent);
        fanNormal = findViewById(R.id.fanNormal);
        fanMax = findViewById(R.id.fanMax);
        fanManual = findViewById(R.id.fanManual);
        fanManualContainer = findViewById(R.id.fanManualContainer);
        fanSpeedSeekBar = findViewById(R.id.fanSpeedSeekBar);
        fanSpeedText = findViewById(R.id.fanSpeedText);
    }

    private void setupEffectButtons() {
        // Tworzymy przyciski efektow dla kazdego panelu
        createEffectButtons(extEffectsContainer, extEffectBtns, COLOR_EXT, effect -> {
            extEffect = effect;
            updateExtButtons();
            sendExtLed();
        });
        createEffectButtons(intEffectsContainer, intEffectBtns, COLOR_INT, effect -> {
            intEffect = effect;
            updateIntButtons();
            sendIntLed();
        });
        createEffectButtons(fanEffectsContainer, fanEffectBtns, COLOR_FAN, effect -> {
            fanEffect = effect;
            updateFanLedButtons();
            sendFanLed();
        });
        updateExtButtons();
        updateIntButtons();
        updateFanLedButtons();
    }

    private void createEffectButtons(LinearLayout container, Button[] buttons, int activeColor, EffectClickListener listener) {
        int btnHeight = dpToPx(36);
        int numEffects = EFFECT_NAMES.length;
        int numRows = (numEffects + 1) / 2;  // 13 efektow = 7 rzedow
        for (int row = 0; row < numRows; row++) {
            LinearLayout rowLayout = new LinearLayout(this);
            rowLayout.setOrientation(LinearLayout.HORIZONTAL);
            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, btnHeight);
            rowParams.topMargin = (row > 0) ? dpToPx(1) : 0;
            rowLayout.setLayoutParams(rowParams);

            for (int col = 0; col < 2; col++) {
                int idx = row * 2 + col;
                if (idx >= numEffects) break;

                Button btn = new Button(this);
                LinearLayout.LayoutParams btnParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1);
                btnParams.setMargins(col == 0 ? 0 : dpToPx(1), 0, 0, 0);
                btn.setLayoutParams(btnParams);
                btn.setText(EFFECT_NAMES[idx]);
                btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                btn.setTextColor(0xFFFFFFFF);
                btn.setBackgroundColor(COLOR_INACTIVE);
                btn.setPadding(0, 0, 0, 0);
                btn.setAllCaps(false);

                final int effectIdx = idx;
                btn.setOnClickListener(v -> listener.onEffectClick(effectIdx));

                buttons[idx] = btn;
                rowLayout.addView(btn);
            }
            container.addView(rowLayout);
        }
    }

    interface EffectClickListener { void onEffectClick(int effect); }

    private int dpToPx(int dp) {
        return (int) (dp * getResources().getDisplayMetrics().density);
    }

    private void setupListeners() {
        audioBtn.setOnClickListener(v -> {
            if (isConnected) {
                queueCommand("TOGGLE");
                audioStateA = !audioStateA;
                updateAudioButton();
            } else {
                statusText.setText("Brak polaczenia!");
                statusText.setTextColor(0xFFFF5252);
            }
        });
        connectBtn.setOnClickListener(v -> { if (isConnected) disconnect(); else scanForDevice(); });

        extBrightness.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { extBrightnessVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendExtLed(); }
        });
        extSpeed.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { extSpeedVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendExtLed(); }
        });

        intBrightness.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { intBrightnessVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendIntLed(); }
        });
        intSpeed.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { intSpeedVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendIntLed(); }
        });

        fanBrightness.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { fanBrightnessVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendFanLed(); }
        });
        fanSpeed.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { fanSpeedVal = p; }
            public void onStopTrackingTouch(SeekBar sb) { sendFanLed(); }
        });

        fanSilent.setOnClickListener(v -> setFanMode(FAN_SILENT));
        fanNormal.setOnClickListener(v -> setFanMode(FAN_NORMAL));
        fanMax.setOnClickListener(v -> setFanMode(FAN_MAX));
        fanManual.setOnClickListener(v -> setFanMode(FAN_MANUAL));
        fanSpeedSeekBar.setOnSeekBarChangeListener(new SimpleSeekBarListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { currentFanPwmSpeed = p; fanSpeedText.setText(p + "%"); }
            public void onStopTrackingTouch(SeekBar sb) { sendFanPwm(); }
        });

        updateFanPwmButtons();
        updateAudioButton();
    }

    private void setupColorButtons() {
        setupColorRow(extColorsContainer, var -> { extVariant = var; sendExtLed(); });
        setupColorRow(intColorsContainer, var -> { intVariant = var; sendIntLed(); });
        setupColorRow(fanColorsContainer, var -> { fanVariant = var; sendFanLed(); });
    }

    private void setupColorRow(LinearLayout container, ColorClickListener listener) {
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(0, dpToPx(6), 0, dpToPx(2));
        int colorHeight = dpToPx(44);
        int numColors = COLOR_VARIANTS.length;
        int colorsPerRow = (numColors + 1) / 2;  // 7 w pierwszym rzedzie, 6 w drugim

        for (int row = 0; row < 2; row++) {
            LinearLayout rowLayout = new LinearLayout(this);
            rowLayout.setOrientation(LinearLayout.HORIZONTAL);
            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, colorHeight);
            rowParams.topMargin = (row > 0) ? dpToPx(4) : 0;
            rowLayout.setLayoutParams(rowParams);

            int startIdx = row * colorsPerRow;
            int endIdx = Math.min(startIdx + colorsPerRow, numColors);

            for (int i = startIdx; i < endIdx; i++) {
                int var = COLOR_VARIANTS[i];
                Button btn = new Button(this);
                LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1);
                p.setMargins(2, 0, 2, 0);
                btn.setLayoutParams(p);
                btn.setBackgroundColor(VARIANT_COLORS[var]);
                btn.setMinWidth(0);
                btn.setMinHeight(0);
                btn.setPadding(0, 0, 0, 0);
                final int variant = var;
                btn.setOnClickListener(x -> listener.onColorClick(variant));
                rowLayout.addView(btn);
            }
            container.addView(rowLayout);
        }
    }

    interface ColorClickListener { void onColorClick(int variant); }

    private void sendExtLed() { queueCommand("EXTLED:" + extEffect + ":" + extVariant + ":" + extSpeedVal + ":" + extBrightnessVal); }
    private void sendIntLed() { queueCommand("INTLED:" + intEffect + ":" + intVariant + ":" + intSpeedVal + ":" + intBrightnessVal); }
    private void sendFanLed() { queueCommand("FANLED:" + fanEffect + ":" + fanVariant + ":" + fanSpeedVal + ":" + fanBrightnessVal); }

    private void sendFanPwm() {
        if (currentFanMode == FAN_MANUAL) queueCommand("FAN:" + currentFanMode + ":" + ((currentFanPwmSpeed * 255) / 100));
        else queueCommand("FAN:" + currentFanMode);
    }

    private void setFanMode(int mode) {
        currentFanMode = mode;
        fanManualContainer.setVisibility(mode == FAN_MANUAL ? View.VISIBLE : View.GONE);
        updateFanPwmButtons();
        sendFanPwm();
    }

    private void updateExtButtons() {
        for (int i = 0; i < EFFECT_NAMES.length; i++) {
            if (extEffectBtns[i] != null) {
                extEffectBtns[i].setBackgroundColor(i == extEffect ? COLOR_EXT : COLOR_INACTIVE);
            }
        }
    }

    private void updateIntButtons() {
        for (int i = 0; i < EFFECT_NAMES.length; i++) {
            if (intEffectBtns[i] != null) {
                intEffectBtns[i].setBackgroundColor(i == intEffect ? COLOR_INT : COLOR_INACTIVE);
            }
        }
    }

    private void updateFanLedButtons() {
        for (int i = 0; i < EFFECT_NAMES.length; i++) {
            if (fanEffectBtns[i] != null) {
                fanEffectBtns[i].setBackgroundColor(i == fanEffect ? COLOR_FAN : COLOR_INACTIVE);
            }
        }
    }

    private void updateFanPwmButtons() {
        fanSilent.setBackgroundColor(currentFanMode == FAN_SILENT ? COLOR_FAN : COLOR_INACTIVE);
        fanNormal.setBackgroundColor(currentFanMode == FAN_NORMAL ? COLOR_FAN : COLOR_INACTIVE);
        fanMax.setBackgroundColor(currentFanMode == FAN_MAX ? COLOR_FAN : COLOR_INACTIVE);
        fanManual.setBackgroundColor(currentFanMode == FAN_MANUAL ? COLOR_FAN : COLOR_INACTIVE);
    }

    private void updateAudioButton() {
        audioBtn.setBackgroundColor(audioStateA ? 0xFF1565C0 : 0xFFC62828);
        audioBtn.setText(audioStateA ? "A" : "B");
    }

    abstract class SimpleSeekBarListener implements SeekBar.OnSeekBarChangeListener {
        public void onStartTrackingTouch(SeekBar sb) {}
        public void onStopTrackingTouch(SeekBar sb) {}
    }

    // === USB Communication ===

    private void registerUsbReceiver() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        registerReceiver(usbReceiver, filter);
    }

    private void scanForDevice() {
        if (isConnecting || isConnected) return;
        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
        for (UsbDevice device : deviceList.values()) {
            // STM32 USB CDC moze uzywac roznych VID:PID
            // 0x1EAF:0x0004 = Maple (STM32duino)
            // 0x0483:0x5740 = STMicroelectronics CDC
            int vid = device.getVendorId();
            int pid = device.getProductId();
            boolean isSTM32 = (vid == 0x1EAF && pid == 0x0004) ||
                              (vid == 0x0483 && pid == 0x5740);
            if (isSTM32) {
                if (usbManager.hasPermission(device)) {
                    connectToDevice(device);
                } else {
                    isConnecting = true;
                    PendingIntent pi = PendingIntent.getBroadcast(this, 0, new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE);
                    usbManager.requestPermission(device, pi);
                }
                return;
            }
        }
        scheduleReconnect();
    }

    private void connectToDevice(UsbDevice device) {
        isConnecting = false;
        try {
            connection = usbManager.openDevice(device);
            if (connection == null) { scheduleReconnect(); return; }
            serialDevice = new ARGBSerialDevice(device, connection);
            serialDevice.open();
            usbDevice = device;
            isConnected = true;
            runOnUiThread(() -> {
                statusText.setText("Polaczono");
                statusText.setTextColor(0xFF4CAF50);
                connectBtn.setText("ROZLACZ");
                connectBtn.setBackgroundColor(0xFFF44336);
            });
            cancelReconnect();
            handler.postDelayed(() -> queueCommand("STATUS"), 500);
        } catch (Exception e) {
            Log.e(TAG, "Connect error", e);
            scheduleReconnect();
        }
    }

    private void disconnect() {
        isConnected = false;
        if (serialDevice != null) { try { serialDevice.close(); } catch (Exception e) {} serialDevice = null; }
        if (connection != null) { try { connection.close(); } catch (Exception e) {} connection = null; }
        usbDevice = null;
        runOnUiThread(() -> {
            statusText.setText("Rozlaczono");
            statusText.setTextColor(0xFFFF5252);
            connectBtn.setText("POLACZ");
            connectBtn.setBackgroundColor(0xFF4CAF50);
        });
    }

    private void scheduleReconnect() {
        cancelReconnect();
        reconnectRunnable = () -> { if (!isConnected && !isConnecting) scanForDevice(); };
        handler.postDelayed(reconnectRunnable, 3000);
    }

    private void cancelReconnect() {
        if (reconnectRunnable != null) { handler.removeCallbacks(reconnectRunnable); reconnectRunnable = null; }
    }

    private void queueCommand(String cmd) {
        synchronized (commandQueue) { commandQueue.add(cmd); }
        processQueue();
    }

    private void processQueue() {
        if (isSending || !isConnected) return;
        String cmd;
        synchronized (commandQueue) { cmd = commandQueue.poll(); }
        if (cmd == null) return;
        isSending = true;
        executor.execute(() -> {
            try {
                if (serialDevice != null) {
                    serialDevice.write((cmd + "\n").getBytes());
                    Thread.sleep(50);
                }
            } catch (Exception e) {
                Log.e(TAG, "Write error", e);
                handler.post(this::disconnect);
            } finally {
                isSending = false;
                handler.post(this::processQueue);
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        try { unregisterReceiver(usbReceiver); } catch (Exception e) {}
        cancelReconnect();
        disconnect();
        executor.shutdown();
    }
}
