package io.fedlet.mobutil;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.database.Cursor;
import android.provider.OpenableColumns;

// 附件元信息选择（外发 share 同口径：只取 名字/类型/大小，不拷贝内容）。
// Qt 侧 pickmedianav.startFilePick(kind) → start() → 系统选择器（SAF
// ACTION_GET_CONTENT，EXTRA_LOCAL_ONLY 本机文件口径）→ onActivityResult
// 查询元信息 → notifyFilePicked JNI 回调 → messagepage 把元信息文案
// 填入输入框（shareMetaToMessage 同款格式），发送走普通文本消息管线。
// 本 activity 是普通 Activity（ShareReceiverActivity 同款独立体，不继承
// QtActivity）：自带 finish，最坏也只是自身结束，不拖死 Qt 主实例。
// 进程内已加载 qsktox native lib（只能从 Qt 内部进入本页），JNI 符号可解析。
public class PickMediaActivity extends Activity {

    private static final String TAG = "PickMediaActivity";

    // 元信息已取齐 → Qt（pickmedianav.cpp 收口）
    private static native void notifyFilePicked(
        String displayName, String mime, long sizeBytes);

    // C++ 端直达入口（QJniObject callStaticMethod）
    public static void start(Context ctx, String kind) {
        Intent it = new Intent(ctx, PickMediaActivity.class);
        it.putExtra("pickKind", kind);
        ctx.startActivity(it);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String kind = getIntent().getStringExtra("pickKind");
        if (kind == null) {
            kind = "file";
        }
        // pickmedianav.cpp pickMimeOf 同表：image / video / audio / file
        final String mime;
        if ("image".equals(kind)) {
            mime = "image/*";
        } else if ("video".equals(kind)) {
            mime = "video/*";
        } else if ("audio".equals(kind)) {
            mime = "audio/*";
        } else {
            mime = "*/*";
        }

        Intent it = new Intent(Intent.ACTION_GET_CONTENT);
        it.addCategory(Intent.CATEGORY_OPENABLE);
        it.setType(mime);
        it.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
        startActivityForResult(it, 1);
    }

    @Override
    protected void onActivityResult(int reqCode, int resCode, Intent data) {
        super.onActivityResult(reqCode, resCode, data);
        // 取消/失败：静默 finish 不回调（输入框保持原样）
        if (resCode != RESULT_OK || data == null || data.getData() == null) {
            finish();
            return;
        }

        Uri uri = data.getData();
        String name = "attachment";
        long size = 0;
        Cursor c = null;
        try {
            c = getContentResolver().query(uri, null, null, null, null);
            if (c != null && c.moveToFirst()) {
                int ni = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                int si = c.getColumnIndex(OpenableColumns.SIZE);
                if (ni >= 0 && !c.isNull(ni)) {
                    name = c.getString(ni);
                }
                if (si >= 0 && !c.isNull(si)) {
                    size = c.getLong(si);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "query meta failed", e);
        } finally {
            if (c != null) {
                c.close();
            }
        }
        if (name == null || name.isEmpty()) {
            name = "attachment";
        }

        String mime = getContentResolver().getType(uri);
        if (mime == null || mime.isEmpty()) {
            mime = "application/octet-stream";
        }

        notifyFilePicked(name, mime, size);
        finish();
    }
}
