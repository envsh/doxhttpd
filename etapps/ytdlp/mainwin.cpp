#include "compat34.h"
#include "lambdaslot.h"
#include "logic.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qtabwidget.h>
#include <qgroupbox.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qfiledialog.h>
#ifdef QT3_BUILD
#include <qtable.h>
#else
#include <qtablewidget.h>
#endif

/* Qt3 QComboBox::insertItem(str) 追加；Qt4 需要 index，addItem(str) 追加 */
static void comboAdd(QComboBox* cb, const QString& s) {
#ifdef QT3_BUILD
    cb->insertItem(s);
#else
    cb->addItem(s);
#endif
}

/* 布局统一走 compat34 qNewBoxLayout（Qt3/Qt4 ctor 顺序兼容） */

extern "C"
QWidget* createYtdlpMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("yt-dlp 下载器操作台"));
#else
    w->setWindowTitle(qFromUtf8("yt-dlp 下载器操作台"));
#endif
    w->resize(760, 620);

    YtdlpUi* ui = new YtdlpUi();

    QBoxLayout* mainLayout = qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QTabWidget* tabs = new QTabWidget(w);
    mainLayout->addWidget(tabs);

    /* ── Tab 1: 主下载 ── */
    QWidget* dlPage = new QWidget(tabs);
    QBoxLayout* dlLayout = qNewBoxLayout(dlPage, QBoxLayout::TopToBottom, 6, 4);

    dlLayout->addWidget(new QLabel(qFromUtf8("视频地址（每行一个 URL）"), dlPage));
    QTextEdit* urlsEdit = new QTextEdit(dlPage);
    urlsEdit->setReadOnly(false);
    urlsEdit->setMinimumHeight(72);
    dlLayout->addWidget(urlsEdit);

    QBoxLayout* fmtRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    fmtRow->addWidget(new QLabel(qFromUtf8("格式"), dlPage));
    QComboBox* presetH = new QComboBox(dlPage);
    comboAdd(presetH, qFromUtf8("自动（推荐 bestvideo+bestaudio 合流）"));
    comboAdd(presetH, qFromUtf8("best（原生单流，含音轨）"));
    comboAdd(presetH, qFromUtf8("高度上限"));
    comboAdd(presetH, qFromUtf8("自定义 -f"));
    fmtRow->addWidget(presetH, 2);
    fmtRow->addWidget(new QLabel(qFromUtf8("高度≤"), dlPage));
    QSpinBox* heightCap = new QSpinBox(dlPage);
    heightCap->setRange(240, 4320);
#ifdef QT3_BUILD
    heightCap->setLineStep(160);
#else
    heightCap->setSingleStep(160);
#endif
    heightCap->setValue(1080);
    fmtRow->addWidget(heightCap, 1);
    fmtRow->addWidget(new QLabel(qFromUtf8("-f"), dlPage));
    QLineEdit* fmtCustom = new QLineEdit(dlPage);
    fmtRow->addWidget(fmtCustom, 2);
    dlLayout->addLayout(fmtRow);

    QBoxLayout* audioRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QCheckBox* extractAudio = new QCheckBox(qFromUtf8("提取音轨 -x"), dlPage);
    audioRow->addWidget(extractAudio);
    audioRow->addWidget(new QLabel(qFromUtf8("格式"), dlPage));
    QComboBox* audioFmt = new QComboBox(dlPage);
    comboAdd(audioFmt, "best");
    comboAdd(audioFmt, "mp3");
    comboAdd(audioFmt, "m4a");
    comboAdd(audioFmt, "opus");
    comboAdd(audioFmt, "flac");
    comboAdd(audioFmt, "wav");
    comboAdd(audioFmt, "vorbis");
    audioRow->addWidget(audioFmt, 1);
    audioRow->addWidget(new QLabel(qFromUtf8("音质"), dlPage));
    QComboBox* audioQual = new QComboBox(dlPage);
    comboAdd(audioQual, "best");
    comboAdd(audioQual, "0");
    comboAdd(audioQual, "2");
    comboAdd(audioQual, "5");
    comboAdd(audioQual, "10");
    audioRow->addWidget(audioQual, 1);
    dlLayout->addLayout(audioRow);

    QBoxLayout* dirRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    dirRow->addWidget(new QLabel(qFromUtf8("保存目录"), dlPage));
    QLineEdit* dirEdit = new QLineEdit(dlPage);
    dirRow->addWidget(dirEdit, 3);
    QPushButton* dirPick = new QPushButton(qFromUtf8("选择…"), dlPage);
    dirRow->addWidget(dirPick);
    dlLayout->addLayout(dirRow);

    QBoxLayout* arcRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QCheckBox* useArchive = new QCheckBox(qFromUtf8("去重档案 --download-archive"), dlPage);
    arcRow->addWidget(useArchive);
    QLineEdit* archiveEdit = new QLineEdit(dlPage);
    arcRow->addWidget(archiveEdit, 3);
    QPushButton* archivePick = new QPushButton(qFromUtf8("选择…"), dlPage);
    arcRow->addWidget(archivePick);
    dlLayout->addLayout(arcRow);

    QBoxLayout* plRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    plRow->addWidget(new QLabel(qFromUtf8("播放列表项 -I"), dlPage));
    QLineEdit* itemsSpec = new QLineEdit(dlPage);
    plRow->addWidget(itemsSpec, 2);
    plRow->addWidget(new QLabel(qFromUtf8("空=整单；如 3,5,8-12"), dlPage), 1);
    dlLayout->addLayout(plRow);

    QBoxLayout* presetRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    presetRow->addWidget(new QLabel(qFromUtf8("预设"), dlPage));
    QComboBox* presetCombo = new QComboBox(dlPage);
    comboAdd(presetCombo, qFromUtf8("(选择预设…占位)"));
    presetRow->addWidget(presetCombo, 3);
    QPushButton* btnPresetSave = new QPushButton(qFromUtf8("存为…"), dlPage);
    presetRow->addWidget(btnPresetSave);
    QPushButton* btnPresetDel = new QPushButton(qFromUtf8("删除"), dlPage);
    presetRow->addWidget(btnPresetDel);
    dlLayout->addLayout(presetRow);

    QBoxLayout* startRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QPushButton* btnStop = new QPushButton(qFromUtf8("停止"), dlPage);
    btnStop->setEnabled(false);
    startRow->addStretch(1);
    startRow->addWidget(btnStop);
    QPushButton* btnStart = new QPushButton(qFromUtf8("开始下载"), dlPage);
    startRow->addWidget(btnStart);
    dlLayout->addLayout(startRow);
    dlLayout->addStretch(1);
    tabs->addTab(dlPage, qFromUtf8("主下载"));

    /* ── Tab 2: 网络登录 ── */
    QWidget* netPage = new QWidget(tabs);
    QBoxLayout* netLayout = qNewBoxLayout(netPage, QBoxLayout::TopToBottom, 6, 4);

    QBoxLayout* cookieRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    cookieRow->addWidget(new QLabel(qFromUtf8("cookies 文件"), netPage));
    QLineEdit* cookieEdit = new QLineEdit(netPage);
    cookieRow->addWidget(cookieEdit, 3);
    QPushButton* cookiePick = new QPushButton(qFromUtf8("选择…"), netPage);
    cookieRow->addWidget(cookiePick);
    netLayout->addLayout(cookieRow);

    QBoxLayout* credRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    credRow->addWidget(new QLabel(qFromUtf8("用户名"), netPage));
    QLineEdit* userEdit = new QLineEdit(netPage);
    credRow->addWidget(userEdit, 2);
    credRow->addWidget(new QLabel(qFromUtf8("密码"), netPage));
    QLineEdit* passEdit = new QLineEdit(netPage);
    passEdit->setEchoMode(QLineEdit::Password);
    credRow->addWidget(passEdit, 2);
    netLayout->addLayout(credRow);

    QBoxLayout* proxyRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    proxyRow->addWidget(new QLabel(qFromUtf8("代理 --proxy"), netPage));
    QLineEdit* proxyEdit = new QLineEdit(netPage);
    proxyRow->addWidget(proxyEdit, 3);
    netLayout->addLayout(proxyRow);

    QBoxLayout* rateRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    rateRow->addWidget(new QLabel(qFromUtf8("限速 --limit-rate"), netPage));
    QLineEdit* rateEdit = new QLineEdit(netPage);
    rateRow->addWidget(rateEdit, 1);
    rateRow->addWidget(new QLabel(qFromUtf8("重试次数"), netPage));
    QSpinBox* retriesSpin = new QSpinBox(netPage);
    retriesSpin->setRange(0, 20);
    retriesSpin->setValue(10);
    rateRow->addWidget(retriesSpin, 1);
    rateRow->addWidget(new QLabel(qFromUtf8("并发分段 -N"), netPage));
    QSpinBox* concSpin = new QSpinBox(netPage);
    concSpin->setRange(1, 32);
    concSpin->setValue(4);
    rateRow->addWidget(concSpin, 1);
    rateRow->addStretch(1);
    netLayout->addLayout(rateRow);

    QBoxLayout* dlExtRowL = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    dlExtRowL->addWidget(new QLabel(qFromUtf8("外部下载器 --downloader"), netPage));
    QComboBox* dlExt = new QComboBox(netPage);
    comboAdd(dlExt, qFromUtf8("原生 (yt-dlp 内置)"));
    comboAdd(dlExt, "aria2c");
    comboAdd(dlExt, "ffmpeg");
    comboAdd(dlExt, "curl");
    dlExtRowL->addWidget(dlExt, 1);
    dlExtRowL->addStretch(1);
    netLayout->addLayout(dlExtRowL);

    netLayout->addStretch(1);
    tabs->addTab(netPage, qFromUtf8("网络登录"));

    /* ── Tab 3: 后处理 ── */
    QWidget* postPage = new QWidget(tabs);
    QBoxLayout* postLayout = qNewBoxLayout(postPage, QBoxLayout::TopToBottom, 6, 4);

    QCheckBox* embedSubs = new QCheckBox(qFromUtf8("内嵌字幕 --embed-subs"), postPage);
    QCheckBox* embedThumb = new QCheckBox(qFromUtf8("内嵌封面 --embed-thumbnail"), postPage);
    QCheckBox* embedMeta = new QCheckBox(qFromUtf8("元数据 --embed-metadata"), postPage);
    QCheckBox* embedChapters = new QCheckBox(qFromUtf8("章节 --embed-chapters"), postPage);
    QBoxLayout* embedRow1 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    embedRow1->addWidget(embedSubs);
    embedRow1->addWidget(embedThumb);
    QBoxLayout* embedRow2 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    embedRow2->addWidget(embedMeta);
    embedRow2->addWidget(embedChapters);
    postLayout->addLayout(embedRow1);
    postLayout->addLayout(embedRow2);

    QBoxLayout* subRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    subRow->addWidget(new QLabel(qFromUtf8("字幕语言 --sub-langs"), postPage));
    QLineEdit* subLangsEdit = new QLineEdit(postPage);
    subRow->addWidget(subLangsEdit, 2);
    subRow->addWidget(new QLabel(qFromUtf8("转码 --convert-subs"), postPage));
    QComboBox* convertSubs = new QComboBox(postPage);
    comboAdd(convertSubs, qFromUtf8("不转换"));
    comboAdd(convertSubs, "srt");
    comboAdd(convertSubs, "ass");
    comboAdd(convertSubs, "vtt");
    comboAdd(convertSubs, "lrc");
    subRow->addWidget(convertSubs, 1);
    postLayout->addLayout(subRow);

    QBoxLayout* mergeRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    mergeRow->addWidget(new QLabel(qFromUtf8("合流容器 --merge-output-format"), postPage));
    QComboBox* mergeFmt = new QComboBox(postPage);
    comboAdd(mergeFmt, qFromUtf8("不指定"));
    comboAdd(mergeFmt, "mkv");
    comboAdd(mergeFmt, "mp4");
    comboAdd(mergeFmt, "webm");
    comboAdd(mergeFmt, "avi");
    comboAdd(mergeFmt, "flv");
    mergeRow->addWidget(mergeFmt, 1);
    mergeRow->addStretch(1);
    postLayout->addLayout(mergeRow);

    QBoxLayout* sbRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QCheckBox* sbRemove = new QCheckBox(qFromUtf8("SponsorBlock 去段"), postPage);
    sbRow->addWidget(sbRemove);
    QLineEdit* sbCatsEdit = new QLineEdit(postPage);
    sbCatsEdit->setText(qFromUtf8("sponsor,intro,outro,selfpromo,interaction"));
    sbRow->addWidget(sbCatsEdit, 3);
    postLayout->addLayout(sbRow);

    QBoxLayout* ppaRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    ppaRow->addWidget(new QLabel(qFromUtf8("--ppa 后处理器参数"), postPage));
    QLineEdit* ppaEdit = new QLineEdit(postPage);
    ppaRow->addWidget(ppaEdit, 3);
    postLayout->addLayout(ppaRow);

    postLayout->addStretch(1);
    tabs->addTab(postPage, qFromUtf8("后处理"));

    /* ── Tab 4: 格式实测 ── */
    QWidget* probePage = new QWidget(tabs);
    QBoxLayout* probeLayout = qNewBoxLayout(probePage, QBoxLayout::TopToBottom, 6, 4);

    QBoxLayout* probeRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QLineEdit* probeUrl = new QLineEdit(probePage);
    probeRow->addWidget(probeUrl, 3);
    QPushButton* btnProbe = new QPushButton(qFromUtf8("探测格式"), probePage);
    probeRow->addWidget(btnProbe);
    QPushButton* btnFill = new QPushButton(qFromUtf8("回填选中"), probePage);
    probeRow->addWidget(btnFill);
    probeLayout->addLayout(probeRow);

    QLabel* probeStatus = new QLabel(qFromUtf8("输入 URL 后点击探测；表格点击行可回填格式 ID"), probePage);
    probeLayout->addWidget(probeStatus);

    YtdlpTable* formatsTable = 0;
    QStringList fmtHeaders;
    fmtHeaders << qFromUtf8("ID") << qFromUtf8("容器")
               << qFromUtf8("分辨率") << qFromUtf8("帧率")
               << qFromUtf8("总码率") << qFromUtf8("编码") << qFromUtf8("大小");
#ifdef QT3_BUILD
    formatsTable = new QTable(0, 7, probePage);
    for (int i = 0; i < 7; i++) {
        formatsTable->horizontalHeader()->setLabel(i, fmtHeaders[i]);
        formatsTable->setColumnReadOnly(i, true);
    }
    formatsTable->setColumnWidth(5, 200);
#else
    formatsTable = new QTableWidget(0, 7, probePage);
    formatsTable->setHorizontalHeaderLabels(fmtHeaders);
    formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
#endif
    probeLayout->addWidget(formatsTable, 1);
    tabs->addTab(probePage, qFromUtf8("格式实测"));

    /* ── Tab 5: 任务 ── */
    QWidget* taskPage = new QWidget(tabs);
    QBoxLayout* taskLayout = qNewBoxLayout(taskPage, QBoxLayout::TopToBottom, 6, 4);

    QBoxLayout* cmdRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QLineEdit* cmdPreview = new QLineEdit(taskPage);
    cmdPreview->setReadOnly(true);
    cmdPreview->setText(qFromUtf8("(命令预览：改动任一选项后自动更新)"));
    cmdRow->addWidget(cmdPreview, 3);
    QPushButton* cmdCopy = new QPushButton(qFromUtf8("复制"), taskPage);
    cmdRow->addWidget(cmdCopy);
    taskLayout->addLayout(cmdRow);

    YtdlpTable* taskTable = 0;
    QStringList taskHeaders;
    taskHeaders << qFromUtf8("地址") << qFromUtf8("状态")
                << qFromUtf8("进度") << qFromUtf8("速度")
                << qFromUtf8("剩余") << qFromUtf8("输出");
#ifdef QT3_BUILD
    taskTable = new QTable(0, 6, taskPage);
    for (int i = 0; i < 6; i++) {
        taskTable->horizontalHeader()->setLabel(i, taskHeaders[i]);
        taskTable->setColumnReadOnly(i, true);
    }
    taskTable->setColumnWidth(0, 210);
    taskTable->setColumnWidth(5, 220);
#else
    taskTable = new QTableWidget(0, 6, taskPage);
    taskTable->setHorizontalHeaderLabels(taskHeaders);
    taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
#endif
    taskLayout->addWidget(taskTable, 2);

    QTextEdit* histLog = new QTextEdit(taskPage);
    histLog->setReadOnly(true);
    histLog->setMinimumHeight(100);
    taskLayout->addWidget(histLog, 1);
    tabs->addTab(taskPage, qFromUtf8("任务"));

    /* ── 底部状态横条 ── */
    QBoxLayout* barRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QLabel* versionLabel = new QLabel(qFromUtf8("yt-dlp 探测中…"), w);
    barRow->addWidget(versionLabel, 3);
    mainLayout->addLayout(barRow);

    /* ── 注入控件聚合 ── */
    ui->urlsEdit = urlsEdit;
    ui->presetH = presetH;
    ui->heightCap = heightCap;
    ui->fmtCustom = fmtCustom;
    ui->extractAudio = extractAudio;
    ui->audioFmt = audioFmt;
    ui->audioQual = audioQual;
    ui->dirEdit = dirEdit;
    ui->dirPick = dirPick;
    ui->useArchive = useArchive;
    ui->archiveEdit = archiveEdit;
    ui->archivePick = archivePick;
    ui->itemsSpec = itemsSpec;
    ui->presetCombo = presetCombo;
    ui->btnPresetSave = btnPresetSave;
    ui->btnPresetDel = btnPresetDel;
    ui->btnStart = btnStart;
    ui->btnStop = btnStop;
    ui->cookieEdit = cookieEdit;
    ui->cookiePick = cookiePick;
    ui->userEdit = userEdit;
    ui->passEdit = passEdit;
    ui->proxyEdit = proxyEdit;
    ui->rateEdit = rateEdit;
    ui->retriesSpin = retriesSpin;
    ui->concSpin = concSpin;
    ui->dlExt = dlExt;
    ui->embedSubs = embedSubs;
    ui->embedThumb = embedThumb;
    ui->embedMeta = embedMeta;
    ui->embedChapters = embedChapters;
    ui->subLangsEdit = subLangsEdit;
    ui->convertSubs = convertSubs;
    ui->mergeFmt = mergeFmt;
    ui->sbRemove = sbRemove;
    ui->sbCatsEdit = sbCatsEdit;
    ui->ppaEdit = ppaEdit;
    ui->probeUrl = probeUrl;
    ui->btnProbe = btnProbe;
    ui->btnFill = btnFill;
    ui->probeStatus = probeStatus;
    ui->formatsTable = formatsTable;
    ui->cmdPreview = cmdPreview;
    ui->taskTable = taskTable;
    ui->histLog = histLog;
    ui->versionLabel = versionLabel;

    YtdlpLogic* logic = new YtdlpLogic(w);
    logic->setWidgets(ui);

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: YtdlpMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
