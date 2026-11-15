#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "compat34.h"
#include <qstring.h>

#define PLUGIN_TYPE_UIAPP 0
#define PLUGIN_TYPE_NOUI  1

/* UI 插件窗口的显式状态（App 级；不依赖 widget 指针是否为空来判断） */
enum PluginWinState {
    PW_None    = 0,   /* 未创建 / 已销毁 */
    PW_Hidden  = 1,   /* 已创建，当前隐藏（用户点 X 后） */
    PW_Visible = 2    /* 已创建，当前显示 */
};

struct PluginInfo {
    QString name;
    QString version;
    QString description;
    QString soPath;
    int     type;
    void*   handle;
    bool    enabled;

    void*  (*create)(void* parent);
    void   (*destroy)(void* widget);
    QWidget* widget;
    int      winState;        /* PluginWinState */

    int    (*nouiInit)(void);
    void   (*nouiUninit)(void);
};

class PluginLoader {
public:
    static void scanPlugins();
    static void rescan();

    static int  uiappCount();
    static int  nouiCount();
    static const PluginInfo& uiappAt(int i);
    static const PluginInfo& nouiAt(int i);

    static QWidget* createUiApp(int index, QWidget* parent);
    static void     destroyUiApp(int index);
    static void     closeAllUiApps();
    static void     shutdownAll();      /* 应用退出：销毁全部窗口后再 dlclose */

    /* 显式状态查询/更新（菜单标识与事件过滤器使用） */
    static int  winStateAt(int index);
    static void setWinStateForPath(const QString& soPath, int state);

    /* 菜单结构变更计数（启用/禁用、重扫），主菜单据此重建 */
    static int  generation();
    static void bumpGeneration();

    static void initNouiPlugins();
    static void uninitNouiPlugins();

    static void setEnabled(int index, bool on);
    static bool isEnabled(int index);

    static void loadState();
    static void saveState();
};

QStringList pluginBaseDirs();

#endif
