package io.fedlet.mobutil;

import android.app.Activity;
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
}
