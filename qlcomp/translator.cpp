#include "translator.h"
#include "cJSON.h"
#include "compat34.h"
#include <qapplication.h>
#include <qtextstream.h>
#if QT_VERSION < 0x060000
#include <qtextcodec.h>
#endif

Translator& Translator::instance() {
    static Translator instance;
    return instance;
}

Translator::Translator() : m_root(nullptr), m_currentLang("zh-CN") {
    // 1. 可执行文件目录下的 lang/
    QString appPath;
#ifdef QT3_BUILD
    appPath = qApp->applicationFilePath();
#else
    appPath = QCoreApplication::applicationFilePath();
#endif
    int lastSlash = qLastIndexOf(appPath, "/");
    if (lastSlash >= 0) {
        m_searchPaths.append(appPath.left(lastSlash + 1) + "lang");
    }

    // 2. 当前工作目录
    m_searchPaths.append("lang");

    // 3. 可执行文件上级目录的 qltox/lang/
    if (lastSlash >= 0) {
        int secondSlash = qLastIndexOf(appPath.left(lastSlash), "/");
        if (secondSlash >= 0) {
            m_searchPaths.append(appPath.left(secondSlash + 1) + "qltox/lang");
            // 4. 项目根目录的 lang/（exe=build3/qltox → ../../lang/）
            int thirdSlash = qLastIndexOf(appPath.left(secondSlash), "/");
            if (thirdSlash >= 0) {
                m_searchPaths.append(appPath.left(thirdSlash + 1) + "lang");
            }
        }
    }

    loadLanguage("zh-CN");
}

void Translator::addTranslationPath(const QString& path) {
    if (!m_searchPaths.contains(path))
        m_searchPaths.append(path);
}

Translator::~Translator() {
    if (m_root) { cJSON_Delete((cJSON*)m_root); }
}

bool Translator::loadLanguage(const QString& langCode) {
    QString filepath;
    for (const QString& dir : m_searchPaths) {
        QString p = dir + "/" + langCode + ".json";
        QFile file(p);
        if (file.exists()) {
            filepath = p;
            break;
        }
    }

    if (filepath.isEmpty()) {
        qWarning("Language file not found for: %s (tried %d paths)", 
                 qToUtf8(langCode).data(), m_searchPaths.size());
        for (int i = 0; i < (int)m_searchPaths.size(); ++i) {
            qWarning("  Path %d: %s", i, qToUtf8(m_searchPaths[i] + "/" + langCode + ".json").data());
        }
        return false;
    }
    
    QFile file(filepath);
    file.close();
    if (!qOpenReadOnly(file)) {
        qWarning("Cannot open language file: %s", qToUtf8(filepath).data());
        return false;
    }
    
    QTextStream stream(&file);
#if QT_VERSION < 0x060000
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
    QString jsonStr;
    while (!stream.atEnd()) {
        jsonStr += stream.readLine();
    }
    file.close();
    qWarning("Language file loaded: %s", qToUtf8(filepath).data());
    
    cJSON* newRoot = cJSON_Parse(qToUtf8(jsonStr).data());
    if (!newRoot) {
        qWarning("Failed to parse JSON: %s", qToUtf8(filepath).data());
        return false;
    }
    
    if (m_root) { cJSON_Delete((cJSON*)m_root); }
    m_root = newRoot;
    m_currentLang = langCode;
    
    emit languageChanged();
    qWarning("Language loaded: %s", qToUtf8(langCode).data());
    return true;
}

QString Translator::t(const QString& key, const QStringList& args) const {
    if (!m_root) {
        qWarning("Translator: m_root is nullptr, returning key: %s", qToUtf8(key).data());
        return key;
    }
    
    // 解析点号分隔的键
    QStringList parts = qSplit(key, ".");
    cJSON* current = (cJSON*)m_root;
    
    // 调试输出
    // qWarning("DEBUG t: key=%s, parts.size=%d, m_root=%p", qToUtf8(key).data(), parts.size(), m_root);
    
    for (int i = 0; i < (int)parts.size(); ++i) {
        // qWarning("DEBUG t: i=%d, part=%s, current=%p, type=%d", i, qToUtf8(parts[i]).data(), current, ((cJSON*)current)->type);
        current = cJSON_GetObjectItem(current, qToUtf8(parts[i]).data());
        // qWarning("DEBUG t: after cJSON_GetObjectItem, current=%p, type=%d", current, current ? ((cJSON*)current)->type : -1);
        if (!current) {
            qWarning("Translation missing: %s (failed at part %d: %s)", 
                     qToUtf8(key).data(), i, qToUtf8(parts[i]).data());
            // 调试：打印父对象的所有键
            if (i > 0) {
                cJSON* parent = (cJSON*)m_root;
                for (int j = 0; j < i; ++j) {
                    parent = cJSON_GetObjectItem(parent, qToUtf8(parts[j]).data());
                }
                if (parent) {
                    qWarning("DEBUG t: parent object keys:");
                    // cJSON 没有直接遍历对象键的 API，但我们可以打印整个对象
                    char* parentStr = cJSON_Print(parent);
                    qWarning("DEBUG t: parent=%s", parentStr);
                    free(parentStr);
                }
            }
            return key;
        }
    }
    
    if (!cJSON_IsString(current)) {
        qWarning("Translation is not a string: %s", qToUtf8(key).data());
        return key;
    }
    
    QString result = qFromUtf8(cJSON_GetStringValue(current));
    
    // 替换占位符 {0}, {1}...
    for (int i = 0; i < (int)args.size(); ++i) {
        result.replace(QString("{%1}").arg(i), args[i]);
    }
    
#ifdef QT3_BUILD
    // Qt3 的 QString::arg() 只支持 %1 %2 格式，不支持 {0} {1}
    result.replace("{0}", "%1");
    result.replace("{1}", "%2");
    result.replace("{2}", "%3");
    result.replace("{3}", "%4");
    result.replace("{4}", "%5");
#endif
    
    return result;
}
