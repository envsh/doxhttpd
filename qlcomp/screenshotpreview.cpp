#include "screenshotpreview.h"
#include "compat34.h"
#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qlayout.h>
#include <qfileinfo.h>
#include <qfiledialog.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qimage.h>
#include <qbuffer.h>
#include <qclipboard.h>
#include <qfile.h>
#else
#include <QLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QApplication>
#include <QDesktopWidget>
#include <QImage>
#include <QBuffer>
#include <QClipboard>
#include <QMimeData>
#include <QFile>
#endif

#ifdef QT3_BUILD
// 剪贴板 PNG 数据源（仿 photoviewer.cpp PhotoClipSource，无 Q_OBJECT，不涉 moc）
class ShotClipSource : public QMimeSource {
public:
    ShotClipSource(const QByteArray& pngBytes) : m_png(pngBytes) {}
    virtual const char* format(int n) const {
        return (n == 0) ? "image/png" : 0;
    }
    virtual QByteArray encodedData(const char* fmt) const {
        if (fmt && qstrcmp(fmt, "image/png") == 0) { return m_png; }
        return QByteArray();
    }
private:
    QByteArray m_png;
};
#endif

// 魔数探测图片类型；无法识别返回空串（仿 photoviewer.cpp:48-55）
static QString sShotImageType(const QString& filePath) {
    QFile f(filePath);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly)) { return QString(); }
    char buf[12];
    Q_LONG got = f.readBlock(buf, 12);   // Qt3: readBlock，无 read(qint64)
    if (got < 12) { return QString(); }
    const uchar* p = (const uchar*)buf;
#else
    if (!f.open(QIODevice::ReadOnly)) { return QString(); }
    QByteArray head = f.read(12);
    if (head.size() < 12) { return QString(); }
    const uchar* p = (const uchar*)head.data();
#endif
    if (p[0] == 0xFF && p[1] == 0xD8) { return QString("JPEG"); }
    if (p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G') { return QString("PNG"); }
    if (p[0] == 'G' && p[1] == 'I' && p[2] == 'F') { return QString("GIF"); }
    if (memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "WEBP", 4) == 0) { return QString("WEBP"); }
    return QString();
}

// 人类可读文件大小（仿 photoviewer.cpp formatBytes）
// Qt3 用 Q_ULLONG（qglobal.h 导出）；Qt4 各版本不统一，用 qulonglong
#ifdef QT3_BUILD
typedef Q_ULLONG ShotFileSize;
#else
typedef qulonglong ShotFileSize;
#endif
static QString sShotHumanSize(ShotFileSize bytes) {
    if (bytes < 1024) { return QString("%1 B").arg(bytes); }
    double kb = (double)bytes / 1024.0;
    if (kb < 1024.0) { return QString("%1 KB").arg(kb, 0, 'f', 1); }
    double mb = kb / 1024.0;
    return QString("%1 MB").arg(mb, 0, 'f', 2);
}

ScreenshotPreviewDialog::ScreenshotPreviewDialog(const QString& filePath, QWidget* parent)
#ifdef QT3_BUILD
    : QDialog(parent, "screenshot_preview")
#else
    : QDialog(parent)
#endif
    , m_filePath(filePath)
    , m_sent(false)
{
    m_pixmap.load(filePath);

    // 标题显示临时图片完整路径
#ifdef QT3_BUILD
    setCaption(m_filePath);
#else
    setWindowTitle(m_filePath);
#endif

    setupUi();
}

ScreenshotPreviewDialog::~ScreenshotPreviewDialog() {
    removeTempFile();    // 兜底3：父窗口销毁/App 退出
}

void ScreenshotPreviewDialog::setupUi() {
    setMinimumSize(400, 300);
    resize(640, 480);

    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 8, 0);

    m_imageLabel = new QLabel(this);
    if (!m_pixmap.isNull()) {
#ifdef QT3_BUILD
        // Qt3: scale via QImage::smoothScale (QPixmap has no scale method)
        QImage img = m_pixmap.convertToImage();
        int ow = img.width();
        int oh = img.height();
        int mw = 600;
        int mh = 380;
        int dw = QApplication::desktop()->width() - 80;
        int dh = QApplication::desktop()->height() - 160;
        if (dw < mw) { mw = dw; }
        if (dh < mh) { mh = dh; }
        double rw = (double)mw / ow;
        double rh = (double)mh / oh;
        double r = (rw < rh) ? rw : rh;
        if (r < 1.0) {
            QImage scaled = img.smoothScale((int)(ow * r), (int)(oh * r), QImage::ScaleMin);
            QPixmap pm;
            pm.convertFromImage(scaled);
            m_imageLabel->setPixmap(pm);
        } else {
            m_imageLabel->setPixmap(m_pixmap);
        }
#else
        QPixmap scaled = m_pixmap.scaled(600, 380, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaled);
#endif
    }
#ifdef QT3_BUILD
    m_imageLabel->setAlignment(AlignCenter);
    m_imageLabel->setBackgroundColor(QColor(32, 32, 32));
#else
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background-color: #202020;");
#endif
    mainLayout->addWidget(m_imageLabel, 1);

    m_metaLabel = new QLabel(this);
#ifdef QT3_BUILD
    m_metaLabel->setAlignment(AlignLeft | AlignVCenter);
    m_metaLabel->setPaletteForegroundColor(QColor(140, 140, 140));
#else
    m_metaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_metaLabel->setStyleSheet("color: #8c8c8c;");
#endif
    m_metaLabel->setText(buildMetaText());
    mainLayout->addWidget(m_metaLabel, 0);

    m_statusLabel = new QLabel(this);
#ifdef QT3_BUILD
    m_statusLabel->setAlignment(AlignLeft | AlignVCenter);
    m_statusLabel->setPaletteForegroundColor(QColor(120, 120, 120));
#else
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet("color: #787878;");
#endif
    mainLayout->addWidget(m_statusLabel, 0);

    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 4);
    mainLayout->addLayout(btnLayout);

    btnLayout->addStretch(1);

    m_sendBtn = new QPushButton("Send", this);
    m_copyBtn = new QPushButton(qFromUtf8("Copy"), this);
    m_saveBtn = new QPushButton("Save As", this);
    m_cancelBtn = new QPushButton("Cancel", this);

    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_copyBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_cancelBtn);

    connect(m_sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(m_copyBtn, SIGNAL(clicked()), this, SLOT(onCopyClicked()));
    connect(m_saveBtn, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
}

QString ScreenshotPreviewDialog::buildMetaText() {
    QString type = sShotImageType(m_filePath);
    if (type.isEmpty()) { type = qFromUtf8("未知"); }
    QString res = QString("%1x%2").arg(m_pixmap.width()).arg(m_pixmap.height());
    QString size = sShotHumanSize((ShotFileSize)QFileInfo(m_filePath).size());
    // 全 ASCII 分隔符，避免 CJK/全角 qFromUtf8 复杂度
    return type + " | " + res + " | " + size;
}

void ScreenshotPreviewDialog::onSendClicked() {
    if (!m_filePath.isEmpty()) {
        m_sent = true;    // 上传管线需要临时文件，关闭时不删除
        emit sendRequested(m_filePath);
    }
    close();
}

void ScreenshotPreviewDialog::onCopyClicked() {
    if (m_pixmap.isNull()) {
        m_statusLabel->setText(qFromUtf8("复制失败：图像为空"));
        return;
    }
#ifdef QT3_BUILD
    QBuffer buf;
    buf.open(IO_WriteOnly);
    QImageIO io(&buf, "PNG");
    io.setImage(m_pixmap.convertToImage());
    io.write();
    buf.close();
    QApplication::clipboard()->setData(
        new ShotClipSource(buf.buffer()));   // 所有权移交剪贴板
#else
    QMimeData* md = new QMimeData();
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    if (!m_pixmap.save(&buf, "PNG")) {
        m_statusLabel->setText(qFromUtf8("复制失败：PNG 编码失败"));
        return;
    }
    md->setData("image/png", buf.buffer());
    md->setImageData(m_pixmap.toImage());    // macOS NSPasteboard TIFF 兜底
    QApplication::clipboard()->setMimeData(md);
#endif
    m_statusLabel->setText(qFromUtf8("已拷贝到剪贴板"));
}

void ScreenshotPreviewDialog::onSaveClicked() {
    QString savePath;

#ifdef QT3_BUILD
    savePath = QFileDialog::getSaveFileName(QString::null, "PNG Image (*.png)", this);
#else
    savePath = QFileDialog::getSaveFileName(this, "Save Screenshot As",
        QString(), "PNG Image (*.png)");
#endif

    if (!savePath.isEmpty()) {
        m_pixmap.save(savePath, "PNG");
        m_statusLabel->setText(qFromUtf8("已保存: ") + savePath);
        emit saveRequested(savePath);
    }
}

void ScreenshotPreviewDialog::onCancelClicked() {
    emit cancelled();
    close();
}

void ScreenshotPreviewDialog::removeTempFile() {
    if (m_sent) { return; }
    if (!m_filePath.isEmpty() && QFile::exists(m_filePath)) {
        QFile::remove(m_filePath);
        m_filePath = QString();      // 幂等：二次调用直接返回
    }
}

void ScreenshotPreviewDialog::closeEvent(QCloseEvent* e) {
    removeTempFile();                // 兜底1：X 角 / close()
    e->accept();
}

void ScreenshotPreviewDialog::reject() {
    removeTempFile();                // 兜底2：Esc（QDialog 不发 closeEvent）
    QDialog::reject();
}
