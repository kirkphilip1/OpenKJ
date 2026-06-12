#include "tagreader.h"
#include <tag.h>
#include <taglib/fileref.h>

TagReader::TagReader(QObject *parent) : QObject(parent)
{
    m_logger = spdlog::get("logger");
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }
    m_duration = 0;
}

TagReader::~TagReader()
{
}

QString TagReader::getArtist()
{
    return m_artist;
}

QString TagReader::getTitle()
{
    return m_title;
}

QString TagReader::getAlbum()
{
    return m_album;
}

QString TagReader::getTrack()
{
    return m_track;
}

unsigned int TagReader::getDuration() const
{
    return m_duration;
}

void TagReader::setMedia(const QString& path)
{
    m_logger->info("{} Getting tags for: {}", m_loggingPrefix, path);
    taglibTags(path);
    m_logger->info("{} Done getting tags for: {}", m_loggingPrefix, path);
}

void TagReader::taglibTags(const QString& path)
{
#ifdef Q_OS_WIN
    std::wstring pathW = path.toStdWString();
    TagLib::FileRef f(pathW.c_str());
#else
    TagLib::FileRef f(path.toLocal8Bit().data());
#endif
    if (!f.isNull() && f.tag())
    {
        m_artist = QString::fromStdString(f.tag()->artist().to8Bit(true));
        m_title = QString::fromStdString(f.tag()->title().to8Bit(true));
        m_album = QString::fromStdString(f.tag()->album().to8Bit(true));
        
        if (f.audioProperties()) {
            m_duration = f.audioProperties()->length() * 1000;
        } else {
            m_duration = 0;
        }

        auto track = f.tag()->track();
        if (track == 0)
            m_track = QString();
        else if (track < 10)
            m_track = "0" + QString::number(track);
        else
            m_track = QString::number(track);
        m_logger->info("{} Taglib result: Artist: {} - Title: {} - Album: {} - Track: {} - Duration: {}", m_loggingPrefix, m_artist, m_title, m_album, m_track, m_duration);
    }
    else
    {
        m_logger->error("{} Taglib was unable to process the specified file", m_loggingPrefix);
        m_artist = QString();
        m_title = QString();
        m_album = QString();
        m_duration = 0;
    }
}
