#include "ttsaudio.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryFile>
#include <QDir>
#include <QUrl>

TtsAudio::TtsAudio()
    : QObject(nullptr)
{
    m_output = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_output);
    m_output->setVolume(1.0);
}

TtsAudio* TtsAudio::instance()
{
    static TtsAudio* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new TtsAudio();
    }
    return s_instance;
}

void TtsAudio::store(const QString& key, const QByteArray& data,
                     const QString& mime, const QString& remoteUrl)
{
    Entry e;
    e.data = data;
    e.mime = mime;
    e.remoteUrl = remoteUrl;
    m_entries.insert(key, e);
}

bool TtsAudio::has(const QString& key) const
{
    return m_entries.contains(key);
}

void TtsAudio::play(const QString& key)
{
    if (!m_entries.contains(key)) {
        return;
    }
    const Entry e = m_entries.value(key);

    m_player->stop();
    if (m_tmpFile) {
        m_tmpFile->close();
        delete m_tmpFile;
        m_tmpFile = nullptr;
    }

    if (!e.data.isEmpty()) {
        QString suffix = QStringLiteral(".mp3");
        if (e.mime.contains(QLatin1String("wav"))) {
            suffix = QStringLiteral(".wav");
        }
        m_tmpFile = new QTemporaryFile(
            QDir::tempPath() + QStringLiteral("/qsktox_tts_XXXXXX") + suffix, this);
        m_tmpFile->setAutoRemove(true);
        if (!m_tmpFile->open()) {
            delete m_tmpFile;
            m_tmpFile = nullptr;
            return;
        }
        m_tmpFile->write(e.data);
        m_tmpFile->flush();
        m_player->setSource(QUrl::fromLocalFile(m_tmpFile->fileName()));
        m_player->play();
    } else if (!e.remoteUrl.isEmpty()) {
        m_player->setSource(QUrl(e.remoteUrl));
        m_player->play();
    }
}
