package io.fedlet.mobutil;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.ContentResolver;
import android.content.pm.PackageManager;
import android.os.Build;
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
