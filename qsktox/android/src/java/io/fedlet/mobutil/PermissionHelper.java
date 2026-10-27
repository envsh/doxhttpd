package io.fedlet.mobutil;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.PowerManager;
import android.provider.Settings;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class PermissionHelper {
    private static final int REQ_NOTIFICATION = 9001;
    private static final int REQ_MEDIA = 9002;
    private static final int REQ_PHONE_CALL = 9003;
    private static final int REQ_BLUETOOTH = 9004;

    public static boolean hasNotificationPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 33)
            return true;
        return ContextCompat.checkSelfPermission(activity,
            Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestNotificationPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 33)
            return;
        if (hasNotificationPermission(activity))
            return;
        ActivityCompat.requestPermissions(activity,
            new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIFICATION);
    }

    public static boolean hasMediaPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 33)
            return true;
        return ContextCompat.checkSelfPermission(activity,
            Manifest.permission.READ_MEDIA_IMAGES) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestMediaPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 33)
            return;
        if (hasMediaPermission(activity))
            return;
        ActivityCompat.requestPermissions(activity,
            new String[]{Manifest.permission.READ_MEDIA_IMAGES}, REQ_MEDIA);
    }

    public static boolean hasPhoneCallPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 29)
            return true;
        return ContextCompat.checkSelfPermission(activity,
            Manifest.permission.ANSWER_PHONE_CALLS) == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean hasReadPhoneStatePermission(Activity activity) {
        return ContextCompat.checkSelfPermission(activity,
            Manifest.permission.READ_PHONE_STATE) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestPhoneCallPermission(Activity activity) {
        java.util.List<String> perms = new java.util.ArrayList<>();
        if (Build.VERSION.SDK_INT >= 29 && !hasPhoneCallPermission(activity)) {
            perms.add(Manifest.permission.ANSWER_PHONE_CALLS);
        }
        if (!hasReadPhoneStatePermission(activity)) {
            perms.add(Manifest.permission.READ_PHONE_STATE);
        }
        if (!perms.isEmpty()) {
            ActivityCompat.requestPermissions(activity,
                perms.toArray(new String[0]), REQ_PHONE_CALL);
        }
    }

    // ── 电池优化白名单（dataSync 6h 超时的跨版本解法，2026-10）──
    // 白名单后走非 FGS 保活路径：不进 dataSync 计时体系，无 6h 上限
    //（Android 14 险档免疫；15/≤13 本就无涉）。
    // 注意：不走 REQUEST_IGNORE_BATTERY_OPTIMIZATIONS 直弹——那需要
    // manifest 权限声明（明令不改 manifest），故引导到设置列表页，
    // 用户手动勾选 qsktox。
    public static boolean isBatteryWhitelisted(Activity activity) {
        PowerManager pm = activity.getSystemService(PowerManager.class);
        return pm != null
            && pm.isIgnoringBatteryOptimizations(activity.getPackageName());
    }

    // 仅首次引导（SharedPreferences 记忆；已白名单/已提示过均直接返回）
    public static void requestBatteryWhitelist(Activity activity) {
        if (isBatteryWhitelisted(activity)) {
            return;
        }
        SharedPreferences sp = activity.getSharedPreferences(
            "mobutil_prefs", Context.MODE_PRIVATE);
        if (sp.getBoolean("battery_whitelist_prompted", false)) {
            return;
        }
        sp.edit().putBoolean("battery_whitelist_prompted", true).apply();
        try {
            activity.startActivity(new Intent(
                Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS));
        } catch (Exception e) {
            android.util.Log.w("qsktox",
                "[PermissionHelper] battery whitelist settings page failed: " + e);
            // 极端 OEM 无设置页：静默跳过（保活退回 FGS 路径仍可工作）
        }
    }

    // ── 蓝牙连接权限 (仅 Android 12 / API 31+ 需运行时申请) ──
    public static boolean hasBluetoothConnectPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 31)
            return true;
        return ContextCompat.checkSelfPermission(activity,
            Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestBluetoothConnectPermission(Activity activity) {
        if (Build.VERSION.SDK_INT < 31)
            return;
        if (hasBluetoothConnectPermission(activity))
            return;
        ActivityCompat.requestPermissions(activity,
            new String[]{Manifest.permission.BLUETOOTH_CONNECT}, REQ_BLUETOOTH);
    }

    // ── 取手机名: BluetoothAdapter ← Secure.bluetooth_name ← Global.device_name ← Build.MODEL ──
    public static String getDeviceName(Activity activity) {
        try {
            if (hasBluetoothConnectPermission(activity)) {
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                if (adapter != null) {
                    String bt = adapter.getName();
                    if (bt != null && !bt.isEmpty())
                        return bt;
                }
            }
        } catch (SecurityException ignored) {
        }

        ContentResolver cr = activity.getContentResolver();

        String n = Settings.Secure.getString(cr, "bluetooth_name");
        if (n != null && !n.isEmpty())
            return n;

        if (Build.VERSION.SDK_INT >= 25) {
            n = Settings.Global.getString(cr, Settings.Global.DEVICE_NAME);
            if (n != null && !n.isEmpty())
                return n;
        }

        String model = Build.MODEL;
        return (model != null && !model.isEmpty()) ? model : "";
    }
}
