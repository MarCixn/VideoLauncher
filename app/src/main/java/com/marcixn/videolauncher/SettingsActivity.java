package com.marcixn.videolauncher;

import android.app.Activity;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Toast;

public class SettingsActivity extends Activity {

    private SharedPreferences prefs;
    private CheckBox showClockCheckbox;
    private CheckBox showMusicInfoCheckbox;
    private EditText folderPreset1, folderPreset2, folderPreset3, folderPreset4;
    private CheckBox folder1Fade, folder2Fade, folder3Fade, folder4Fade;
    private CheckBox folder1Shuffle, folder2Shuffle, folder3Shuffle, folder4Shuffle;
    private CheckBox showAndroidStats, showPcStats;
    private RadioGroup statsDisplayMode;
    private RadioButton statsTextMode, statsBarsMode;
    private RadioGroup playModeGroup;
    private RadioButton loopRadio, sequentialRadio;
    private RadioGroup durationGroup;
    private RadioButton fullVideoRadio, partialRadio;
    private EditText secondsInput;
    private Button saveBtn;

    // Style buttons
    private Button styleCyberpunk, styleNeon, styleMinimal, styleRed;
    private String selectedStyle = "cyberpunk";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );

        setContentView(R.layout.activity_settings);

        prefs = getSharedPreferences("VideoLauncher", MODE_PRIVATE);

        initViews();
        loadCurrentSettings();
        setupListeners();
    }

    private void initViews() {
        showClockCheckbox = findViewById(R.id.showClockCheckbox);
        showMusicInfoCheckbox = findViewById(R.id.showMusicInfoCheckbox);
        folderPreset1 = findViewById(R.id.folderPreset1);
        folderPreset2 = findViewById(R.id.folderPreset2);
        folderPreset3 = findViewById(R.id.folderPreset3);
        folderPreset4 = findViewById(R.id.folderPreset4);

        // Per-folder options
        folder1Fade = findViewById(R.id.folder1Fade);
        folder2Fade = findViewById(R.id.folder2Fade);
        folder3Fade = findViewById(R.id.folder3Fade);
        folder4Fade = findViewById(R.id.folder4Fade);
        folder1Shuffle = findViewById(R.id.folder1Shuffle);
        folder2Shuffle = findViewById(R.id.folder2Shuffle);
        folder3Shuffle = findViewById(R.id.folder3Shuffle);
        folder4Shuffle = findViewById(R.id.folder4Shuffle);

        // Stats options
        showAndroidStats = findViewById(R.id.showAndroidStats);
        showPcStats = findViewById(R.id.showPcStats);
        statsDisplayMode = findViewById(R.id.statsDisplayMode);
        statsTextMode = findViewById(R.id.statsTextMode);
        statsBarsMode = findViewById(R.id.statsBarsMode);

        playModeGroup = findViewById(R.id.playModeGroup);
        loopRadio = findViewById(R.id.loopRadio);
        sequentialRadio = findViewById(R.id.sequentialRadio);
        durationGroup = findViewById(R.id.durationGroup);
        fullVideoRadio = findViewById(R.id.fullVideoRadio);
        partialRadio = findViewById(R.id.partialRadio);
        secondsInput = findViewById(R.id.secondsInput);
        saveBtn = findViewById(R.id.saveBtn);

        // Style buttons
        styleCyberpunk = findViewById(R.id.styleCyberpunk);
        styleNeon = findViewById(R.id.styleNeon);
        styleMinimal = findViewById(R.id.styleMinimal);
        styleRed = findViewById(R.id.styleRed);
    }

    private void loadCurrentSettings() {
        boolean isLooping = prefs.getBoolean("loopMode", true);
        int duration = prefs.getInt("playDuration", 0);
        boolean showClock = prefs.getBoolean("showClock", true);
        selectedStyle = prefs.getString("style", "cyberpunk");

        // Load folder presets
        folderPreset1.setText(prefs.getString("folderPreset1", "/sdcard/Movies"));
        folderPreset2.setText(prefs.getString("folderPreset2", "/sdcard/Download/wideo"));
        folderPreset3.setText(prefs.getString("folderPreset3", "/sdcard/Download"));
        folderPreset4.setText(prefs.getString("folderPreset4", "/sdcard/DCIM"));

        // Load per-folder options
        folder1Fade.setChecked(prefs.getBoolean("folder1Fade", false));
        folder2Fade.setChecked(prefs.getBoolean("folder2Fade", false));
        folder3Fade.setChecked(prefs.getBoolean("folder3Fade", false));
        folder4Fade.setChecked(prefs.getBoolean("folder4Fade", false));
        folder1Shuffle.setChecked(prefs.getBoolean("folder1Shuffle", false));
        folder2Shuffle.setChecked(prefs.getBoolean("folder2Shuffle", false));
        folder3Shuffle.setChecked(prefs.getBoolean("folder3Shuffle", false));
        folder4Shuffle.setChecked(prefs.getBoolean("folder4Shuffle", false));

        // Load stats options
        showAndroidStats.setChecked(prefs.getBoolean("showAndroidStats", false));
        showPcStats.setChecked(prefs.getBoolean("showPcStats", false));

        // Load stats display mode
        boolean useBars = prefs.getBoolean("statsUseBars", false);
        if (useBars) {
            statsBarsMode.setChecked(true);
        } else {
            statsTextMode.setChecked(true);
        }

        showClockCheckbox.setChecked(showClock);
        showMusicInfoCheckbox.setChecked(prefs.getBoolean("showMusicInfo", true));
        updateStyleButtons();

        if (isLooping) {
            loopRadio.setChecked(true);
        } else {
            sequentialRadio.setChecked(true);
        }

        if (duration == 0) {
            fullVideoRadio.setChecked(true);
            secondsInput.setEnabled(false);
        } else {
            partialRadio.setChecked(true);
            secondsInput.setText(String.valueOf(duration));
            secondsInput.setEnabled(true);
        }

        updateDurationVisibility();
    }

    private void setupListeners() {
        playModeGroup.setOnCheckedChangeListener((group, checkedId) -> {
            updateDurationVisibility();
        });

        durationGroup.setOnCheckedChangeListener((group, checkedId) -> {
            secondsInput.setEnabled(checkedId == R.id.partialRadio);
        });

        saveBtn.setOnClickListener(v -> saveSettings());

        // Style button listeners
        styleCyberpunk.setOnClickListener(v -> {
            selectedStyle = "cyberpunk";
            updateStyleButtons();
        });

        styleNeon.setOnClickListener(v -> {
            selectedStyle = "neon";
            updateStyleButtons();
        });

        styleMinimal.setOnClickListener(v -> {
            selectedStyle = "minimal";
            updateStyleButtons();
        });

        styleRed.setOnClickListener(v -> {
            selectedStyle = "red";
            updateStyleButtons();
        });
    }

    private void updateStyleButtons() {
        // Reset all buttons
        styleCyberpunk.setBackgroundColor(Color.parseColor("#333333"));
        styleNeon.setBackgroundColor(Color.parseColor("#333333"));
        styleMinimal.setBackgroundColor(Color.parseColor("#333333"));
        styleRed.setBackgroundColor(Color.parseColor("#333333"));

        styleCyberpunk.setTextColor(Color.WHITE);
        styleNeon.setTextColor(Color.WHITE);
        styleMinimal.setTextColor(Color.WHITE);
        styleRed.setTextColor(Color.WHITE);

        // Highlight selected
        switch (selectedStyle) {
            case "cyberpunk":
                styleCyberpunk.setBackgroundColor(Color.parseColor("#00FFFF"));
                styleCyberpunk.setTextColor(Color.BLACK);
                break;
            case "neon":
                styleNeon.setBackgroundColor(Color.parseColor("#FF00FF"));
                styleNeon.setTextColor(Color.BLACK);
                break;
            case "minimal":
                styleMinimal.setBackgroundColor(Color.parseColor("#FFFFFF"));
                styleMinimal.setTextColor(Color.BLACK);
                break;
            case "red":
                styleRed.setBackgroundColor(Color.parseColor("#FF0040"));
                styleRed.setTextColor(Color.WHITE);
                break;
        }
    }

    private void updateDurationVisibility() {
        boolean showDuration = sequentialRadio.isChecked();
        durationGroup.setVisibility(showDuration ? View.VISIBLE : View.GONE);
        secondsInput.setVisibility(showDuration ? View.VISIBLE : View.GONE);
        findViewById(R.id.durationLabel).setVisibility(showDuration ? View.VISIBLE : View.GONE);
    }

    private void saveSettings() {
        SharedPreferences.Editor editor = prefs.edit();

        editor.putBoolean("showClock", showClockCheckbox.isChecked());
        editor.putBoolean("showMusicInfo", showMusicInfoCheckbox.isChecked());
        editor.putBoolean("loopMode", loopRadio.isChecked());
        editor.putString("style", selectedStyle);

        // Save folder presets
        String fp1 = folderPreset1.getText().toString().trim();
        String fp2 = folderPreset2.getText().toString().trim();
        String fp3 = folderPreset3.getText().toString().trim();
        String fp4 = folderPreset4.getText().toString().trim();

        editor.putString("folderPreset1", fp1.isEmpty() ? "/sdcard/Movies" : fp1);
        editor.putString("folderPreset2", fp2.isEmpty() ? "/sdcard/Download/wideo" : fp2);
        editor.putString("folderPreset3", fp3.isEmpty() ? "/sdcard/Download" : fp3);
        editor.putString("folderPreset4", fp4.isEmpty() ? "/sdcard/DCIM" : fp4);

        // Save per-folder options
        editor.putBoolean("folder1Fade", folder1Fade.isChecked());
        editor.putBoolean("folder2Fade", folder2Fade.isChecked());
        editor.putBoolean("folder3Fade", folder3Fade.isChecked());
        editor.putBoolean("folder4Fade", folder4Fade.isChecked());
        editor.putBoolean("folder1Shuffle", folder1Shuffle.isChecked());
        editor.putBoolean("folder2Shuffle", folder2Shuffle.isChecked());
        editor.putBoolean("folder3Shuffle", folder3Shuffle.isChecked());
        editor.putBoolean("folder4Shuffle", folder4Shuffle.isChecked());

        // Save stats options
        editor.putBoolean("showAndroidStats", showAndroidStats.isChecked());
        editor.putBoolean("showPcStats", showPcStats.isChecked());
        editor.putBoolean("statsUseBars", statsBarsMode.isChecked());

        // Set current folder to first preset if not already set
        String currentFolder = prefs.getString("videoFolder", "");
        if (currentFolder.isEmpty() ||
            (!currentFolder.equals(fp1) && !currentFolder.equals(fp2) &&
             !currentFolder.equals(fp3) && !currentFolder.equals(fp4))) {
            editor.putString("videoFolder", fp1.isEmpty() ? "/sdcard/Movies" : fp1);
        }

        int duration = 0;
        if (partialRadio.isChecked()) {
            try {
                duration = Integer.parseInt(secondsInput.getText().toString());
            } catch (NumberFormatException e) {
                duration = 10;
            }
        }
        editor.putInt("playDuration", duration);

        editor.apply();

        Toast.makeText(this, "Zapisano!", Toast.LENGTH_SHORT).show();
        finish();
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }
}
