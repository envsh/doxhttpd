#ifndef CHATVIEW_H
#define CHATVIEW_H

#include "compat34.h"
#include "floatingpill.h"
#include "StyleParams.h"
#include "lambdaslot.h"
#include <string>
#include <vector>
#include <deque>
#include <cstdint>
#ifdef QT3_BUILD
#include <qmap.h>
#else
#include <QMap>
#endif
#include <qdatetime.h>
#include <qrect.h>
#include <qwidget.h>
#include <qpainter.h>
#include <qtimer.h>
#ifndef QT3_BUILD
#include <QTimer>
#endif

#if defined(QT3_BUILD)
#include <qmovie.h>
#else
#include <QMovie>
#endif

enum class TransState { None, Scheduled, InFlight, Done };

// Block index for binary-search accelerated Y-position lookup.
// Each block has at most kBlockSize messages; cumulativeHeight is
// the absolute Y of the first message in the block (including kPad).
static const int kBlockSize = 50;

struct MsgBlock {
    int cumulativeHeight;
};

struct ChatElement {
    enum SendState { SendSending, SendSent, SendFailed };
    enum ElementType { Text, Image, File, Video, Gif, Audio };

    ElementType etype;

    // Common
    QString senderName;
    QString senderNickname;
    int     peerNumber;
    QString avatarUrl;
    QString time;
    QString ipAddress;

    // Text only
    QString messageText;
    QString category;          // "self" / "other" / "friend"

    // Translation (Text only)
    QString translatedText;
    QString translateError;
    bool showTranslation;
    TransState transState;
    bool needsTranslateComputed;
    bool needsTranslateResult;
    QRect translateBtnRect;
    QRect sourceBtnRect;

    // Shared media fields (Image / Gif / Video)
    QPixmap scaledDisplay;   // 预缩放到显示尺寸的缓存
    QString caption;
    QString mediaUrl;
    enum DownloadState { NotRequested, InProgress, Completed, Failed };
    DownloadState downloadState;
    QRect thumbnailRect;
    int mediaWidth;
    QRect downloadBtnRect;
    QRect retryBtnRect;
    int mediaHeight;

    // File only
    QString fileName;
    int fileSize;
    int downloadProgress;
    QString localPath;

    // Video only
    int durationSec;
    bool gifLikeVideo;     // ≤3s webm/mp4，当作动图循环展示
    bool pendingPlay;      // 双击触发，下载完成后自动交给播放器

    // Gif only
    QString gifPath;
    QMovie* movie;

    // Layout cache (TG-style per-element)
    short cachedWidth;
    short height;
    uchar firstInGroup : 1;
    SendState sendState;
    int sendmsgseq = -1;
    int64_t dbRowid = 0;      // SQLite rowid，由 DB 加载时填充
    QString messageId;        // 服务端消息 ID（暂未填充）
    QString sendErrorMsg;
    QRect resendIconRect;
    QStringList replyTos;
    QStringList mentions;
    bool redacted = false;

    ChatElement()
        : etype(Text), peerNumber(-1), showTranslation(false)
        , transState(TransState::None), needsTranslateComputed(false), needsTranslateResult(false)
        , downloadState(NotRequested)
        , mediaWidth(0), mediaHeight(0)
        , fileSize(0), downloadProgress(0), durationSec(0), gifLikeVideo(false), pendingPlay(false)
        , movie(nullptr)
        , cachedWidth(-1), height(0), firstInGroup(1), sendState(SendSending) {}

    int calcHeight(int viewWidth, const QFontMetrics& fm, int emojiW, const QFont& baseFont);
    void paint(QPainter& p, int y, int viewWidth, bool isSelected,
               const std::vector<QRect>& selRects,
               const QFontMetrics& fm, int emojiW,
               const QFont& baseFont, const StyleParams::Palette& pal);
    void startGifLikeAnimation(QWidget* parent, int msgIndex);
    void stopGifLikeAnimation();
};

QPixmap makeScaledThumb(const QPixmap& src, int mediaW, int mediaH, int maxContainW);
bool isWebP(const std::string& d);
QPixmap decodeWebP(const std::string& data);

struct LinkSpan {
    int start;
    int end;
    QString url;
};

class LimeScrollBar;
class ChatHistory;

class ChatHistoryObserver {
public:
    virtual ~ChatHistoryObserver() = default;
    virtual void onInsertOne(size_t index) = 0;
    virtual void onInsertRange(size_t start, size_t cnt) = 0;
    virtual void onUpdateOne(size_t index) = 0;
    virtual void onUpdateRange(size_t start, size_t cnt) = 0;
    virtual void onRemoveOne(size_t index) = 0;
    virtual void onRemoveRange(size_t start, size_t cnt) = 0;
};

class ChatView : public QWidget, public ChatHistoryObserver {
    Q_OBJECT
public:
    ChatView(QWidget* parent = 0);
    ~ChatView();

    void setBuffer(ChatHistory* hist);
    void scrollToBottom();
    void scrollBottomIfNeeded();
    ChatElement& messageAt(int index);
    int messageCount() const;
    void setFavRowids(const std::vector<int64_t>& favRowids);
    bool isFavRowid(int64_t rowid) const;
    void setFavRowid(int64_t rowid, bool fav);
    void updateElement(int msgIndex);
    void relayout();
    void repaintMessageElement(int msgIndex);
    void onGifFrameUpdated(int msgIndex);

    // ChatHistoryObserver
    void onInsertOne(size_t index) override;
    void onInsertRange(size_t start, size_t cnt) override;
    void onUpdateOne(size_t index) override;
    void onUpdateRange(size_t start, size_t cnt) override;
    void onRemoveOne(size_t index) override;
    void onRemoveRange(size_t start, size_t cnt) override;

    static const int kAvatarSize   = 42;
    static const int kPad          = 8;
    static const int kMsgSpacing   = 8;
    static const int kBubbleHPad   = 12;
    static const int kBubbleVPad   = 8;
    static const int kBubbleRadius = 8;

protected:
    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent* event);
    void wheelEvent(QWheelEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void contextMenuEvent(QContextMenuEvent* event);

signals:
    void translateClicked(int msgIndex);
    void sourceClicked(int msgIndex);
    void mentionClicked(const QString& senderName, const QString& nickName);
    void retryClicked(int msgIndex, const QString& mediaUrl, const QString& source);
    void downloadNeeded(int msgIndex, const QString& mediaUrl);
    void openFullSizeImage(int msgIndex, const QString& mediaUrl);
    void openMediaPlayer(int msgIndex);
    void resendMessage(int msgIndex);
    void autoTranslateRequested(int msgIndex, const QString& text, const QString& toLang);
    void replyRequested(int msgIndex);
    void editRequested(int msgIndex);
    void deleteRequested(int msgIndex);
    void redactRequested(int msgIndex);
    void favoriteClicked(int msgIndex);
    void forwardClicked(int msgIndex);

private slots:
    void onScrollChanged(int value);
    void onAnimTick();
    void flushScrollUpdate();

private:
    bool m_scrollUpdatePending = false;
    void scheduleScrollUpdate();
    void rebuildBlocks();
    void flushEviction();
    void _appendToBlocks(int elementHeight);
    void _prependToBlocks(int count);
    void _updateBlockFor(int idx, int oldHeight);
    void _removeFromBlocks(int idx);
    void resetCanvas();
    int blockForIndex(int msgIndex) const;
    int msgAbsY(int msgIndex) const;
    int findByAbsY(int absY) const;
    /// 全量刷新：切换上下文、滚动、resetCanvas、relayout、全选等结构变化场景
    void updateFull();
    /// 增量刷新：appendMessage、pill 悬浮/计数、selection 拖拽等局部脏矩形场景
    void updateRect(const QRect& r);
    /// 计算消息在视口中的矩形，msgIndex=-1 返回空矩形
    QRect messageRect(int msgIndex) const;
    int contentWidth() const;
    int charWidth(uint32_t cp);
    void manageGifLikeAnimations();
    std::pair<int,int> visibleMessageRange() const;
    void _updateScrollState();

    // Selection and link helpers
    int findMessageAtY(int y) const;
    int charPosAt(int msgIndex, int localX, int localY);
    void selectWordAt(int msgIndex, int charPos);
    void selectLineAt(int msgIndex, int charPos);
    std::vector<QRect> selectionRects(int msgIndex);
    QString selectedText() const;
    std::vector<LinkSpan> extractLinks(const QString& text);
    void copySelectedText();
    void copyFullMessage(int msgIndex);
    void showRawData(int msgIndex);

    // Click tracking for double/triple click
    int m_clickCount;
    int m_clickMsgIndex;
    QTime m_clickTime;

    // Selection state
    int m_selMsgIndex;
    int m_selStart;
    int m_selEnd;
    bool m_selecting;

    ChatHistory* m_history;
    // 当前聊天的收藏 rowid 集合（聊天切换时一次性加载，切换即时更新；菜单/点击零查库）
    QMap<int64_t, int> m_favRowids;
    std::vector<char> m_gifFrameUpdated;
    int m_totalHeight;
    int m_scrollPos;
    std::vector<MsgBlock> m_blocks;
    LimeScrollBar* m_vScrollBar;
    QTimer* m_animTimer;
    FloatingPill m_scrollDownPill;
    QPixmap m_backBuffer;

    QFontMetrics m_fm;
    int m_emojiW;
    uint8_t m_ascW[128];
    uint8_t* m_bmpW;
    int m_scrollDelta;
    int m_evictLow = 0;
    int m_evictHigh = 0;

};

#endif
