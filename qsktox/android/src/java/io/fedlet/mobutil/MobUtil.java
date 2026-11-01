package io.fedlet.mobutil;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.view.WindowInsets;
import android.graphics.Point;
import java.nio.charset.StandardCharsets;

public class MobUtil {

    // 字节流按 charsetName 解码后重新编码为 UTF-8；失败返回 null（不向 native 抛异常）。
    public static byte[] convertToUtf8(byte[] data, String charsetName) {
        if (data == null) { return null; }
        try {
            return new String(data, charsetName).getBytes(StandardCharsets.UTF_8);
        } catch (Exception e) {
            return null;
        }
    }

    public static int statusBarHeight(Activity activity) {
        WindowInsets insets = activity.getWindow().getDecorView()
            .getRootWindowInsets();
        if (insets != null) {
            return insets.getInsets(WindowInsets.Type.statusBars()).top;
        }
        return 0;
    }

    public static int navBarHeight(Activity activity) {
        WindowInsets insets = activity.getWindow().getDecorView()
            .getRootWindowInsets();
        if (insets != null) {
            return insets.getInsets(WindowInsets.Type.navigationBars()).bottom;
        }
        return 0;
    }

    public static int[] availableScreenSize(Activity activity) {
        Point size = new Point();
        activity.getWindowManager().getDefaultDisplay()
            .getSize(size);
        return new int[]{size.x, size.y};
    }

    public static void toast(Activity activity, String message) {
        android.widget.Toast.makeText(activity, message,
            android.widget.Toast.LENGTH_SHORT).show();
    }

    // 是否处于充电状态（minSdk=23，BatteryManager.isCharging 自 API 23 可用）
    public static boolean isCharging(Context context) {
        try {
            BatteryManager bm = (BatteryManager)
                context.getSystemService(Context.BATTERY_SERVICE);
            return bm != null && bm.isCharging();
        } catch (Exception e) {
            return false;
        }
    }

    // 启发式 Root 判定：常见 su 路径 / PATH 中 which su / test-keys 构建。
    // App 沙箱内无法 100% 判定，仅作状态提示。
    public static boolean isRooted() {
        String[] suPaths = {
            "/system/bin/su", "/system/xbin/su", "/system/sbin/su",
            "/sbin/su", "/su/bin/su", "/data/local/bin/su",
            "/data/local/xbin/su", "/system/sd/xbin/su",
            "/system/bin/failsafe/su", "/system/su", "/vendor/bin/su"
        };
        for (String p : suPaths) {
            try {
                if (new java.io.File(p).exists()) return true;
            } catch (Exception ignored) {
            }
        }
        try {
            Process proc = Runtime.getRuntime().exec(new String[]{"which", "su"});
            if (proc.waitFor() == 0) {
                java.io.BufferedReader r = new java.io.BufferedReader(
                    new java.io.InputStreamReader(proc.getInputStream()));
                String line = r.readLine();
                if (line != null && line.contains("su")) return true;
            }
        } catch (Exception ignored) {
        }
        try {
            String tags = android.os.Build.TAGS;
            if (tags != null && tags.contains("test-keys")) return true;
        } catch (Exception ignored) {
        }
        return false;
    }

    // 电量百分数（0-100）；未知/失败返回 -1。
    // 首选 BATTERY_PROPERTY_CAPACITY；不>0 时 fallback 粘滞 ACTION_BATTERY_CHANGED 广播。
    public static int getBatteryLevel(Context context) {
        try {
            BatteryManager bm = (BatteryManager)
                context.getSystemService(Context.BATTERY_SERVICE);
            if (bm != null) {
                int level = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY);
                if (level > 0)
                    return level;
            }
            Intent si = context.registerReceiver(null,
                new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (si != null) {
                int lv = si.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                int sc = si.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
                if (lv >= 0 && sc > 0)
                    return (lv * 100) / sc;
            }
            return -1;
        } catch (Exception e) {
            return -1;
        }
    }

    // 电池温度，单位 0.1℃（Android 原始约定：295=29.5℃）；未知/失败返回 -1。
    // 温度只在粘滞 ACTION_BATTERY_CHANGED 广播的 EXTRA_TEMPERATURE 里。
    public static int getBatteryTemperature(Context context) {
        try {
            Intent si = context.registerReceiver(null,
                new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (si != null) {
                int t = si.getIntExtra(BatteryManager.EXTRA_TEMPERATURE,
                    Integer.MIN_VALUE);
                if (t != Integer.MIN_VALUE)
                    return t;
            }
            return -1;
        } catch (Exception e) {
            return -1;
        }
    }
}
