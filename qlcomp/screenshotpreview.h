#ifndef SCREENSHOT_PREVIEW_H
#define SCREENSHOT_PREVIEW_H

// Reference:
//   PhotoViewer (existing): qltox/photoviewer.h
//   QDialog pattern: qlcomp/ConfigDialog.h

#include "compat34.h"
#include <qdialog.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qlabel.h>
#include <qpushbutton.h>
#ifdef QT3_BUILD
#include <qevent.h>
#else
#include <QCloseEvent>
#endif

class ScreenshotPreviewDialog : public QDialog {
    Q_OBJECT
public:
    ScreenshotPreviewDialog(const QString& filePath, QWidget* parent = nullptr);
    ~ScreenshotPreviewDialog();

signals:
    void sendRequested(const QString& filePath);
    void saveRequested(const QString& filePath);
    void cancelled();

protected:
    void closeEvent(QCloseEvent* e) override;   // 兜底1：X 角 / close()
    void reject() override;                     // 兜底2：Esc 键

private slots:
    void onSendClicked();
    void onCopyClicked();
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUi();
    void removeTempFile();
    QString buildMetaText();            // 拼 "类型 | 分辨率 | 大小"

    QString m_filePath;
    QPixmap m_pixmap;
    QLabel* m_imageLabel;
    QLabel* m_metaLabel;                // 常驻：类型 | 分辨率 | 大小
    QLabel* m_statusLabel;
    QPushButton* m_sendBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
    bool m_sent;                  // 已发送则保留临时文件（上传管线要用）
};

#endif
