package io.fedlet.mobutil;

import android.content.Intent;
import android.os.Bundle;
import org.qtproject.qt.android.bindings.QtActivity;

public class ShareActivity extends QtActivity {

    // 通知 Qt：pending_shares 目录有新的分享落盘，供 Qt 扫描并打开联系人选择页
    private static native void notifyPendingShare();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        // 热启动：receiver 已把分享内容落盘，通知 Qt 扫描 pending_shares
        notifyPendingShare();
    }
}
