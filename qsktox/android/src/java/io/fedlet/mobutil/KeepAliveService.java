package io.fedlet.mobutil;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

public class KeepAliveService extends Service {
    private static final int NOTIF_ID = 1001;
    private static final String CHANNEL_ID = "qsktox_keepalive";

    // 白名单（忽略电池优化）用户 → 普通 started service，不起前台——
    // 机制上无 FGS/dataSync 6h 计时（口径 2026-10 网搜核对）
    private static boolean isWhitelisted(Context ctx) {
        PowerManager pm = ctx.getSystemService(PowerManager.class);
        return pm != null
            && pm.isIgnoringBatteryOptimizations(ctx.getPackageName());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    // ⚠️ 不覆写 onTimeout（2026-10 网搜核对结论）——防止后人误以为漏加：
    // dataSync 的超时回调是 onTimeout(int,int)，仅【Android 15+ 设备 且
    // targetSdk≥35】才会触发。本服务当前组合（dataSync + targetSdk=34）：
    //   · Android 14 机：6h 到点系统直接 cached（进程杀，无回调机会，不可捕获）
    //   · Android 15 机：target 不达标豁免新超时行为（无回调、不崩）
    // 任何 onTimeout 覆写在当前组合下都是永不触发的死代码。
    // sdk 版本不动（compileSdk 33 / targetSdk 34）。
    // 未来真要加固：先 build-android.sh:131 compileSdk→35、:138 targetSdk→35
    //（连带 edge-to-edge 强制 UI 适配），再加双参+单参两个覆盖。
    // 白名单双路径可直接绕开该体系（见 isWhitelisted：白名单下不进
    // FGS，dataSync 计时无从谈起）。
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (isWhitelisted(this)) {
            // 普通 service：不起前台。startForeground(FGS) 才会进 dataSync
            // 计时；不调无副作用——5s 超时反过来只管 startForegroundService
            // 起的服务（本服务唯一调用方 KeepAlive 前台启动，白名单下走
            // startService，见下）
            return START_NOT_STICKY;
        }
        Intent launchIntent = getPackageManager()
            .getLaunchIntentForPackage(getPackageName());
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        PendingIntent pi = PendingIntent.getActivity(
            this, 0, launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        Notification notif = new Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("qsktox")
            .setContentText("Running in background")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setOngoing(true)
            .setContentIntent(pi)
            .build();
        startForeground(NOTIF_ID, notif);
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "qsktox Service",
                NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("Keep app alive in background");
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }
    }

    public static void startService(Context ctx) {
        Intent intent = new Intent(ctx, KeepAliveService.class);
        // 白名单 → 普通 startService（app 前台态启动无 O+ 后台启动限制；
        // 唯一调用方 KeepAlive::start 在 app 启动时触发）
        if (!isWhitelisted(ctx)
            && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ctx.startForegroundService(intent);
        } else {
            ctx.startService(intent);
        }
    }

    public static void stopService(Context ctx) {
        ctx.stopService(new Intent(ctx, KeepAliveService.class));
    }
}
