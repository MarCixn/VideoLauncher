package com.marcixn.videolauncher;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;

public class KeepAliveService extends Service {

    private Handler handler = new Handler();
    private static final long INTERVAL = 5000; // Every 5 seconds

    private Runnable keepAliveRunnable = new Runnable() {
        @Override
        public void run() {
            pingPhoneApp();
            handler.postDelayed(this, INTERVAL);
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        handler.post(keepAliveRunnable);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY; // Restart if killed
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void pingPhoneApp() {
        try {
            // Broadcast 1: BT service action
            Intent btIntent = new Intent("com.nwd.btbroad.service.ACTION_START");
            btIntent.setPackage("com.nwd.android.phone");
            sendBroadcast(btIntent);

            // Broadcast 2: Explicit to receiver
            Intent recvIntent = new Intent();
            recvIntent.setComponent(new ComponentName(
                "com.nwd.android.phone",
                "com.visualframe.btphone.BtBroadcastReceiver"
            ));
            recvIntent.setAction("com.nwd.btbroad.service.ACTION_START");
            sendBroadcast(recvIntent);

            // Broadcast 3: Generic wake
            Intent wakeIntent = new Intent();
            wakeIntent.setPackage("com.nwd.android.phone");
            wakeIntent.setAction("android.intent.action.USER_PRESENT");
            sendBroadcast(wakeIntent);

        } catch (Exception e) {
            // Ignore
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
    }
}
