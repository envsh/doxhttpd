#include "plugin_loader.h"
#include "limelog.h"
#include <qdir.h>
#include <qfileinfo.h>
#include <qsettings.h>
#include <qstringlist.h>
#include <qevent.h>
#include <qmap.h>
#include <dlfcn.h>

static QList<PluginInfo> s_uiapps;
static QList<PluginInfo> s_nouis;

/* 窗口与显式状态按 soPath 存放：rescan 清空列表时不会丢失存活窗口 */
struct PluginRuntime {
    QWidget* widget;
    int winState;
    PluginRuntime() : widget(0), winState(PW_None) {}
};
static QMap<QString, PluginRuntime> s_runtime;
static int s_generation = 0;

/* 事件过滤器：Show/Hide/Destroy 同步显式状态（菜单“已打开”标识的数据源） */
class PluginWinWatcher : public QObject {
public:
    explicit PluginWinWatcher(const QString& path, QObject* parent = 0)
        : QObject(parent), m_path(path) {}
protected:
    bool eventFilter(QObject* obj, QEvent* e) {
        if (e->type() == QEvent::Show) {
            PluginLoader::setWinStateForPath(m_path, PW_Visible);
        } else if (e->type() == QEvent::Hide) {
            PluginLoader::setWinStateForPath(m_path, PW_Hidden);
        } else if (e->type() == QEvent::Destroy) {
            PluginLoader::setWinStateForPath(m_path, PW_None);
        }
        return QObject::eventFilter(obj, e);
    }
private:
    QString m_path;
};

// Qt3 QPtrList<PluginInfo>: must heap-allocate items (list stores pointers)
// Call setAutoDelete(true) so clear() frees items.
#ifdef QT3_BUILD
static inline void initPluginLists() {
    s_uiapps.setAutoDelete(true);
    s_nouis.setAutoDelete(true);
}
#endif

// Qt3 QPtrList<PluginInfo>::operator[] returns PluginInfo*, not PluginInfo&
#ifdef QT3_BUILD
static inline PluginInfo& pluginRef(QList<PluginInfo>& list, int i) { return *list[i]; }
static inline const PluginInfo& pluginRef(const QList<PluginInfo>& list, int i) {
    return *const_cast<QList<PluginInfo>&>(list)[i];
}
#else
static inline PluginInfo& pluginRef(QList<PluginInfo>& list, int i) { return list[i]; }
static inline const PluginInfo& pluginRef(const QList<PluginInfo>& list, int i) { return list[i]; }
#endif

QStringList pluginBaseDirs() {
    QStringList dirs;

#ifdef QT3_BUILD
    dirs.append(qCurrDir() + "/plugins");
    dirs.append(qAppDir() + "/plugins");
    dirs.append(qAppDir() + "/../plugins");
#if defined(__APPLE__)
    dirs.append("/Library/Application Support/qltox/plugins");
#else
    dirs.append("/usr/local/share/qltox/plugins");
    dirs.append("/usr/share/qltox/plugins");
#endif
#if defined(__APPLE__)
    dirs.append(qGetHomePath() + "/Library/Application Support/qltox/plugins");
#else
    dirs.append(qGetHomePath() + "/.config/qltox/plugins");
#endif
#else
    dirs.append(qCurrDir() + "/plugins4");
    dirs.append(qAppDir() + "/plugins4");
    dirs.append(qAppDir() + "/../plugins4");
#if defined(__APPLE__)
    dirs.append("/Library/Application Support/qltox/plugins4");
#else
    dirs.append("/usr/local/share/qltox/plugins4");
    dirs.append("/usr/share/qltox/plugins4");
#endif
#if defined(__APPLE__)
    dirs.append(qGetHomePath() + "/Library/Application Support/qltox/plugins4");
#else
    dirs.append(qGetHomePath() + "/.config/qltox/plugins4");
#endif
#endif

    return dirs;
}

static QString settingsFilePath() {
    return qGetHomePath() + "/.q3tox_settings";
}

static bool stateFromSettings(const QString& name, int type) {
    QString key;
    if (type == PLUGIN_TYPE_UIAPP) {
        key = "plugins/disabled_uiapps";
    } else {
        key = "plugins/disabled_noui";
    }

#ifdef QT3_BUILD
    QSettings s(QSettings::Ini);
    QString val = s.readEntry(key);
#else
    QSettings s(settingsFilePath(), QSettings::IniFormat);
    QString val = s.value(key).toString();
#endif

    if (val.isEmpty()) {
        return true;
    }
    return !val.contains(name);
}

void PluginLoader::loadState() {
}

void PluginLoader::saveState() {
#ifdef QT3_BUILD
    QSettings s(QSettings::Ini);
#else
    QSettings s(settingsFilePath(), QSettings::IniFormat);
#endif

    QStringList disabledUiapps;
    for (int i = 0; i < s_uiapps.count(); i++) {
        PluginInfo& info = pluginRef(s_uiapps, i);
        if (!info.enabled) {
            disabledUiapps.append(info.name);
        }
    }
#ifdef QT3_BUILD
    s.writeEntry("plugins/disabled_uiapps", disabledUiapps.join(","));
#else
    s.setValue("plugins/disabled_uiapps", disabledUiapps.join(","));
#endif

    QStringList disabledNouis;
    for (int i = 0; i < s_nouis.count(); i++) {
        PluginInfo& info = pluginRef(s_nouis, i);
        if (!info.enabled) {
            disabledNouis.append(info.name);
        }
    }
#ifdef QT3_BUILD
    s.writeEntry("plugins/disabled_noui", disabledNouis.join(","));
#else
    s.setValue("plugins/disabled_noui", disabledNouis.join(","));
#endif
}

static void scanDir(const QString& path, int type, QList<PluginInfo>& out) {
    QDir dir(path);
    if (!dir.exists()) {
        ALOG_INFO("[plugins] dir not found, skip:", path);
        return;
    }

    ALOG_INFO("[plugins] scanning:", path);

    QStringList nameFilter;
    if (type == PLUGIN_TYPE_UIAPP) {
        nameFilter << "lib*_ui.so";
    } else {
        nameFilter << "lib*_noui.so";
    }

#ifdef QT3_BUILD
    const QFileInfoList* files = dir.entryInfoList(nameFilter.join(" "), QDir::Files);
    if (!files) {
        return;
    }
    QFileInfoList* filesMut = const_cast<QFileInfoList*>(files);
    for (int i = 0; i < filesMut->count(); i++) {
        QFileInfo* fi = filesMut->at(i);
#else
    QFileInfoList files = dir.entryInfoList(nameFilter, QDir::Files);
    for (int i = 0; i < files.count(); i++) {
        QFileInfo* fi = &files[i];
#endif
        ALOG_INFO("[plugins]   found:", fi->fileName());

#ifdef QT3_BUILD
        void* handle = dlopen(fi->absFilePath().utf8(), RTLD_LAZY);
#else
        void* handle = dlopen(fi->absoluteFilePath().toUtf8().constData(), RTLD_LAZY);
#endif
        if (!handle) {
            ALOG_WARN("[plugins]   dlopen failed:", dlerror());
            continue;
        }

        PluginInfo info;
        info.handle = handle;
#ifdef QT3_BUILD
        info.soPath = fi->absFilePath();
#else
        info.soPath = fi->absoluteFilePath();
#endif
        info.type = type;
        info.widget = 0;
        info.winState = PW_None;
        info.create = 0;
        info.destroy = 0;
        info.nouiInit = 0;
        info.nouiUninit = 0;

        if (type == PLUGIN_TYPE_UIAPP) {
            info.create = (void*(*)(void*))dlsym(handle, "plugin_create");
            info.destroy = (void(*)(void*))dlsym(handle, "plugin_destroy");
            typedef const char* (*NameFn)();
            NameFn nameFn = (NameFn)dlsym(handle, "plugin_name");
            NameFn verFn = (NameFn)dlsym(handle, "plugin_version");
            NameFn descFn = (NameFn)dlsym(handle, "plugin_description");

            if (!nameFn || !verFn || !info.create || !info.destroy) {
                ALOG_WARN("[plugins]   symbol missing in", fi->fileName());
                dlclose(handle);
                continue;
            }
            info.name = QString::fromUtf8(nameFn());
            info.version = QString::fromUtf8(verFn());
            if (descFn) {
                info.description = QString::fromUtf8(descFn());
            }
        } else {
            typedef const char* (*NameFn)();
            NameFn nameFn = (NameFn)dlsym(handle, "noui_name");
            NameFn verFn = (NameFn)dlsym(handle, "noui_version");
            NameFn descFn = (NameFn)dlsym(handle, "noui_description");
            info.nouiInit = (int(*)(void))dlsym(handle, "noui_init");
            info.nouiUninit = (void(*)(void))dlsym(handle, "noui_uninit");

            if (!nameFn || !verFn) {
                ALOG_WARN("[plugins]   symbol missing in", fi->fileName());
                dlclose(handle);
                continue;
            }
            info.name = QString::fromUtf8(nameFn());
            info.version = QString::fromUtf8(verFn());
            if (descFn) {
                info.description = QString::fromUtf8(descFn());
            }
        }

        info.enabled = stateFromSettings(info.name, type);

        ALOG_INFO("[plugins]   loaded:", info.name, "v" + info.version,
                  info.enabled ? "(enabled)" : "(disabled)");
#ifdef QT3_BUILD
        out.append(new PluginInfo(info));
#else
        out.append(info);
#endif
    }
}

void PluginLoader::scanPlugins() {
#ifdef QT3_BUILD
    initPluginLists();
#endif
    s_uiapps.clear();
    s_nouis.clear();

    QStringList dirs = pluginBaseDirs();
    for (int i = 0; i < dirs.count(); i++) {
        scanDir(dirs[i] + "/uiapps", PLUGIN_TYPE_UIAPP, s_uiapps);
        scanDir(dirs[i] + "/noui", PLUGIN_TYPE_NOUI, s_nouis);
    }

    /* 已不存在的插件：其窗口必须销毁（未变化的保留窗口与状态） */
    QStringList keys = s_runtime.keys();
    QStringList gone;
    for (int k = 0; k < keys.count(); ++k) {
        bool found = false;
        for (int i = 0; i < s_uiapps.count(); ++i) {
            if (pluginRef(s_uiapps, i).soPath == keys[k]) { found = true; break; }
        }
        if (!found) { gone.append(keys[k]); }
    }
    for (int k = 0; k < gone.count(); ++k) {
        PluginRuntime& rt = s_runtime[gone[k]];
        if (rt.widget) {
            ALOG_INFO("[plugins] plugin gone, destroy window:", gone[k]);
            rt.widget->deleteLater();
            rt.widget = 0;
        }
        rt.winState = PW_None;
        s_runtime.remove(gone[k]);
    }
    bumpGeneration();

    ALOG_INFO("[plugins] total:", s_uiapps.count(), "uiapp,",
              s_nouis.count(), "noui plugins found");
}

void PluginLoader::rescan() {
    scanPlugins();
}

int PluginLoader::uiappCount() {
    return s_uiapps.count();
}

int PluginLoader::nouiCount() {
    return s_nouis.count();
}

const PluginInfo& PluginLoader::uiappAt(int i) {
    return pluginRef(s_uiapps, i);
}

const PluginInfo& PluginLoader::nouiAt(int i) {
    return pluginRef(s_nouis, i);
}

QWidget* PluginLoader::createUiApp(int index, QWidget* parent) {
    if (index < 0 || index >= s_uiapps.count()) {
        return 0;
    }

    PluginInfo& info = pluginRef(s_uiapps, index);
    PluginRuntime& rt = s_runtime[info.soPath];
    if (rt.widget) {
        rt.widget->show();
        rt.widget->raise();
        rt.winState = PW_Visible;
        return rt.widget;
    }

    if (!info.create) {
        return 0;
    }

    ALOG_INFO("[plugins] createUiApp:", info.name);
    rt.widget = (QWidget*)info.create(parent);
    if (rt.widget) {
        /* 事件过滤器 parent=window：窗口销毁时自动回收 */
        rt.widget->installEventFilter(new PluginWinWatcher(info.soPath, rt.widget));
        rt.winState = PW_Visible;
        rt.widget->show();
    }
    return rt.widget;
}

void PluginLoader::destroyUiApp(int index) {
    if (index < 0 || index >= s_uiapps.count()) {
        return;
    }

    PluginInfo& info = pluginRef(s_uiapps, index);
    PluginRuntime& rt = s_runtime[info.soPath];
    if (rt.widget && info.destroy) {
        info.destroy(rt.widget);
        rt.widget = 0;
    }
    rt.winState = PW_None;
    if (info.handle) {
        dlclose(info.handle);
        info.handle = 0;
    }
}

void PluginLoader::closeAllUiApps() {
    for (int i = 0; i < s_uiapps.count(); i++) {
        PluginInfo& info = pluginRef(s_uiapps, i);
        PluginRuntime& rt = s_runtime[info.soPath];
        if (rt.widget && info.destroy) {
            ALOG_INFO("[plugins] closeAllUiApps: destroying", info.name);
            info.destroy(rt.widget);
            rt.widget = 0;
        }
        rt.winState = PW_None;
    }
}

int PluginLoader::winStateAt(int index) {
    if (index < 0 || index >= s_uiapps.count()) { return PW_None; }
    const PluginInfo& info = pluginRef(s_uiapps, index);
    QMap<QString, PluginRuntime>::iterator it = s_runtime.find(info.soPath);
    if (it == s_runtime.end()) { return PW_None; }
#ifdef QT3_BUILD
    return it.data().winState;
#else
    return it.value().winState;
#endif
}

void PluginLoader::setWinStateForPath(const QString& soPath, int state) {
    s_runtime[soPath].winState = state;
}

/* 应用退出：先销毁全部窗口（插件代码仍在），再卸载 .so */
void PluginLoader::shutdownAll() {
    closeAllUiApps();
    for (int i = 0; i < s_uiapps.count(); i++) {
        PluginInfo& info = pluginRef(s_uiapps, i);
        if (info.handle) { dlclose(info.handle); info.handle = 0; }
    }
}

int PluginLoader::generation() { return s_generation; }
void PluginLoader::bumpGeneration() { ++s_generation; }

void PluginLoader::initNouiPlugins() {
    for (int i = 0; i < s_nouis.count(); i++) {
        PluginInfo& info = pluginRef(s_nouis, i);
        if (!info.enabled || !info.nouiInit) {
            continue;
        }
        int result = info.nouiInit();
        if (result == 0) {
            ALOG_INFO("[plugins] noui_init OK:", info.name);
        } else {
            ALOG_WARN("[plugins] noui_init FAILED:", info.name,
                      "returned", result);
        }
    }
}

void PluginLoader::uninitNouiPlugins() {
    for (int i = 0; i < s_nouis.count(); i++) {
        PluginInfo& info = pluginRef(s_nouis, i);
        if (!info.enabled || !info.nouiUninit) {
            continue;
        }
        info.nouiUninit();
        ALOG_INFO("[plugins] noui_uninit:", info.name);
    }
}

void PluginLoader::setEnabled(int index, bool on) {
    int total = s_uiapps.count() + s_nouis.count();
    if (index < 0 || index >= total) {
        return;
    }

    if (index < s_uiapps.count()) {
        pluginRef(s_uiapps, index).enabled = on;
    } else {
        pluginRef(s_nouis, index - s_uiapps.count()).enabled = on;
    }
    saveState();
    bumpGeneration();
}

bool PluginLoader::isEnabled(int index) {
    int total = s_uiapps.count() + s_nouis.count();
    if (index < 0 || index >= total) {
        return false;
    }

    if (index < s_uiapps.count()) {
        return pluginRef(s_uiapps, index).enabled;
    }
    return pluginRef(s_nouis, index - s_uiapps.count()).enabled;
}
