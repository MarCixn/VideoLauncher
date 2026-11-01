package com.marcixn.videolauncher;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.BaseAdapter;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class AppDrawerActivity extends Activity {

    private GridView appGrid;
    private List<AppInfo> apps = new ArrayList<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );

        setContentView(R.layout.activity_app_drawer);

        appGrid = findViewById(R.id.appGrid);

        loadApps();
        setupGrid();
    }

    private void loadApps() {
        apps.clear();
        PackageManager pm = getPackageManager();

        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);

        List<ResolveInfo> availableApps = pm.queryIntentActivities(intent, 0);

        for (ResolveInfo ri : availableApps) {
            String packageName = ri.activityInfo.packageName;
            if (packageName.equals(getPackageName())) continue;

            AppInfo app = new AppInfo();
            app.label = ri.loadLabel(pm).toString();
            app.packageName = packageName;
            app.icon = ri.loadIcon(pm);
            apps.add(app);
        }

        Collections.sort(apps, (a, b) -> a.label.compareToIgnoreCase(b.label));
    }

    private void setupGrid() {
        appGrid.setAdapter(new AppAdapter());
        appGrid.setOnItemClickListener((parent, view, position, id) -> {
            AppInfo app = apps.get(position);
            Intent launchIntent = getPackageManager()
                .getLaunchIntentForPackage(app.packageName);
            if (launchIntent != null) {
                startActivity(launchIntent);
                finish();
                overridePendingTransition(R.anim.fade_in, R.anim.slide_down_out);
            }
        });
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        overridePendingTransition(R.anim.fade_in, R.anim.slide_down_out);
    }

    static class AppInfo {
        String label;
        String packageName;
        Drawable icon;
    }

    class AppAdapter extends BaseAdapter {
        @Override
        public int getCount() { return apps.size(); }

        @Override
        public AppInfo getItem(int position) { return apps.get(position); }

        @Override
        public long getItemId(int position) { return position; }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view;
            if (convertView == null) {
                view = getLayoutInflater().inflate(R.layout.item_app, parent, false);
            } else {
                view = convertView;
            }

            AppInfo app = getItem(position);
            ImageView icon = view.findViewById(R.id.appIcon);
            TextView label = view.findViewById(R.id.appLabel);

            icon.setImageDrawable(app.icon);
            label.setText(app.label);

            return view;
        }
    }
}
