#ifndef TTS_AUDIO_H
#define TTS_AUDIO_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>

class QMediaPlayer;
class QAudioOutput;
class QTemporaryFile;

// TTS 合成音频缓存 + 播放器（GUI 线程单例）。
// 字节型落临时文件后播放；URL 型直连播放。
class TtsAudio : public QObject
{
    Q_OBJECT
public:
    static TtsAudio* instance();

    void store(const QString& key, const QByteArray& data,
               const QString& mime, const QString& remoteUrl);
    bool has(const QString& key) const;
    void play(const QString& key);

private:
    TtsAudio();
    struct Entry {
        QByteArray data;
        QString mime;
        QString remoteUrl;
    };
    QHash<QString, Entry> m_entries;
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_output = nullptr;
    QTemporaryFile* m_tmpFile = nullptr;
};

#endif // TTS_AUDIO_H
