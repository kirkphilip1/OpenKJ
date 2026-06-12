#include "mediabackend.h"
#include "mzarchive.h"
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <malloc.h>
inline uchar* aligned_alloc_helper(size_t size) {
    return static_cast<uchar*>(_aligned_malloc(size, 64));
}
inline void aligned_free_helper(uchar *ptr) {
    if (ptr) _aligned_free(ptr);
}
#else
inline uchar* aligned_alloc_helper(size_t size) {
    void *ptr = nullptr;
    if (posix_memalign(&ptr, 64, size) != 0) {
        return nullptr;
    }
    return static_cast<uchar*>(ptr);
}
inline void aligned_free_helper(uchar *ptr) {
    if (ptr) free(ptr);
}
#endif

// LibVLC callbacks implementation
unsigned setup_cb(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines) {
    auto *backend = static_cast<MediaBackend*>(*opaque);
    QMutexLocker locker(&backend->m_videoMutex);
    
    memcpy(chroma, "RGBA", 4);
    
    backend->m_videoWidth = *width;
    backend->m_videoHeight = *height;
    
    // Align pitch and lines for SIMD colorspace converter requirements.
    // Stride must be a multiple of 64 bytes (16 pixels of RGBA).
    unsigned int alignedWidth = (*width + 15) & ~15;
    *pitches = alignedWidth * 4;
    *lines = (*height + 31) & ~31;
    backend->m_videoPitch = *pitches;
    
    size_t newSize = static_cast<size_t>(*pitches) * (*lines);
    if (backend->m_videoBufferSize < newSize) {
        if (backend->m_videoBuffer) {
            aligned_free_helper(backend->m_videoBuffer);
        }
        backend->m_videoBuffer = aligned_alloc_helper(newSize);
        if (backend->m_videoBuffer) {
            backend->m_videoBufferSize = newSize;
        } else {
            backend->m_videoBufferSize = 0;
            return 0;
        }
    }
    
    backend->m_hasVideo = true;
    emit backend->hasActiveVideoChanged(true);
    
    return 1;
}

void cleanup_cb(void *opaque) {
    auto *backend = static_cast<MediaBackend*>(opaque);
    QMutexLocker locker(&backend->m_videoMutex);
    // Keep m_videoBuffer allocated to prevent race condition crashes if lock_cb is called during/after cleanup.
    // It will be freed in setup_cb if a larger size is needed, or in the destructor.
    backend->m_videoWidth = 0;
    backend->m_videoHeight = 0;
    backend->m_videoPitch = 0;
    backend->m_hasVideo = false;
    emit backend->hasActiveVideoChanged(false);
}

void* lock_cb(void *opaque, void **pixels) {
    auto *backend = static_cast<MediaBackend*>(opaque);
    backend->m_videoMutex.lock();
    if (backend->m_videoBuffer) {
        *pixels = backend->m_videoBuffer;
    } else {
        static uchar* fallbackBuffer = nullptr;
        if (!fallbackBuffer) {
            fallbackBuffer = aligned_alloc_helper(1920 * 1080 * 4);
        }
        *pixels = fallbackBuffer;
    }
    return nullptr;
}

void unlock_cb(void *opaque, void *picture, void *const *pixels) {
    auto *backend = static_cast<MediaBackend*>(opaque);
    backend->m_videoMutex.unlock();
}

void display_cb(void *opaque, void *picture) {
    auto *backend = static_cast<MediaBackend*>(opaque);
    QMutexLocker locker(&backend->m_videoMutex);
    if (!backend->m_videoBuffer || backend->m_videoWidth <= 0 || backend->m_videoHeight <= 0) {
        return;
    }
    QImage img(backend->m_videoBuffer, backend->m_videoWidth, backend->m_videoHeight, backend->m_videoPitch, QImage::Format_RGBA8888);
    QImage imgCopy = img.copy();
    emit backend->frameReady(imgCopy);
}

// MediaBackend constructor & methods
MediaBackend::MediaBackend(QObject *parent, QString objectName, MediaType type)
    : QObject(parent), m_objName(objectName), m_type(type)
{
    m_loggingPrefix = "[" + objectName.toStdString() + "]";
    m_logger = spdlog::get("logger");
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }

    m_logger->info("{} Initializing LibVLC media backend", m_loggingPrefix);

    // Set plugins directory path on macOS so LibVLC loads codecs correctly
#ifdef Q_OS_MAC
    if (QDir("/Applications/VLC.app/Contents/MacOS/plugins").exists()) {
        qputenv("VLC_PLUGIN_PATH", "/Applications/VLC.app/Contents/MacOS/plugins");
    } else if (QDir("/opt/homebrew/lib/vlc/plugins").exists()) {
        qputenv("VLC_PLUGIN_PATH", "/opt/homebrew/lib/vlc/plugins");
    } else if (QDir("/usr/local/lib/vlc/plugins").exists()) {
        qputenv("VLC_PLUGIN_PATH", "/usr/local/lib/vlc/plugins");
    }
#endif


    std::vector<const char*> args;
    args.push_back("--no-video-title-show");
    args.push_back("--quiet");

    bool useMono = (m_type == Karaoke) ? m_settings.audioDownmix() :
                   (m_type == BackgroundMusic) ? m_settings.audioDownmixBm() : false;
    m_isMonoInitialized = useMono;
    if (useMono) {
        args.push_back("--audio-filter=scaletempo,mono");
    } else {
        args.push_back("--audio-filter=scaletempo");
    }

    m_vlcInstance = libvlc_new(args.size(), args.data());
    if (!m_vlcInstance) {
        m_logger->error("{} Failed to create LibVLC instance", m_loggingPrefix);
        return;
    }

    m_vlcPlayer = libvlc_media_player_new(m_vlcInstance);
    if (!m_vlcPlayer) {
        m_logger->error("{} Failed to create LibVLC media player", m_loggingPrefix);
        return;
    }

    setupVlcCallbacks();

    // Set up AudioFader
    m_fader = new AudioFader(this);
    m_fader->setObjName(objectName);
    connect(m_fader, &AudioFader::volumeChanged, this, [this](double volMult) {
        if (m_vlcPlayer) {
            int vol = static_cast<int>(m_volume * volMult);
            if (m_muted) vol = 0;
            libvlc_audio_set_volume(m_vlcPlayer, vol);
        }
    });
    connect(m_fader, &AudioFader::fadeComplete, this, [this]() {
        if (m_fader->state() == AudioFader::FadedOut) {
            rawStop();
        }
    });

    // Populate devices and apply saved settings on startup
    getOutputDevices();
    if (m_type == Karaoke) {
        setAudioOutputDevice(m_settings.audioOutputDevice());
    } else if (m_type == BackgroundMusic) {
        setAudioOutputDevice(m_settings.audioOutputDeviceBm());
    } else {
        setAudioOutputDevice("Default");
    }

    connect(&m_vlcEventTimer, &QTimer::timeout, this, &MediaBackend::eventTimer_timeout);
    m_vlcEventTimer.start(100);
}

MediaBackend::~MediaBackend() {
    m_vlcEventTimer.stop();
    rawStop();
    if (m_vlcPlayer) {
        libvlc_media_player_release(m_vlcPlayer);
        m_vlcPlayer = nullptr;
    }
    if (m_vlcInstance) {
        libvlc_release(m_vlcInstance);
        m_vlcInstance = nullptr;
    }
    m_tempDir.reset();

    if (m_videoBuffer) {
        aligned_free_helper(m_videoBuffer);
        m_videoBuffer = nullptr;
    }
}

void MediaBackend::setupVlcCallbacks() {
    if (!m_vlcPlayer) return;
    libvlc_video_set_callbacks(m_vlcPlayer, lock_cb, unlock_cb, display_cb, this);
    libvlc_video_set_format_callbacks(m_vlcPlayer, setup_cb, cleanup_cb);
}

bool MediaBackend::isSilent() {
    return false;
}

void MediaBackend::setAudioOutputDevice(const AudioOutputDevice &device) {
    if (m_vlcPlayer) {
        if (device.id.isEmpty() || device.name == "Default") {
            libvlc_audio_output_device_set(m_vlcPlayer, nullptr, nullptr);
        } else {
            libvlc_audio_output_device_set(m_vlcPlayer, nullptr, device.id.toUtf8().constData());
        }
    }
}

void MediaBackend::setAudioOutputDevice(const QString &deviceName) {
    if (!m_vlcPlayer) return;
    if (m_audioOutputDevices.empty()) {
        getOutputDevices();
    }
    QString deviceId;
    for (const auto &dev : m_audioOutputDevices) {
        if (dev.name == deviceName) {
            deviceId = dev.id;
            break;
        }
    }
    if (deviceId.isEmpty() || deviceName == "Default") {
        libvlc_audio_output_device_set(m_vlcPlayer, nullptr, nullptr);
    } else {
        libvlc_audio_output_device_set(m_vlcPlayer, nullptr, deviceId.toUtf8().constData());
    }
}

void MediaBackend::setVideoOutputWidgets(const std::vector<QWidget*>& surfaces) {
    for (auto* display : m_videoSinks) {
        if (display) {
            disconnect(this, &MediaBackend::frameReady, display, &VideoDisplay::updateFrame);
            display->clearFrame();
        }
    }
    m_videoSinks.clear();
    for (auto* surface : surfaces) {
        if (auto* display = qobject_cast<VideoDisplay*>(surface)) {
            m_videoSinks.push_back(display);
            connect(this, &MediaBackend::frameReady, display, &VideoDisplay::updateFrame, Qt::QueuedConnection);
        }
    }
}

void MediaBackend::setVideoEnabled(const bool &enabled) {
    m_videoEnabled = enabled;
}

bool MediaBackend::hasActiveVideo() {
    return m_hasVideo;
}

qint64 MediaBackend::position() {
    if (m_vlcPlayer) {
        libvlc_time_t t = libvlc_media_player_get_time(m_vlcPlayer);
        return (t >= 0) ? t : 0;
    }
    return 0;
}

qint64 MediaBackend::duration() {
    if (m_vlcPlayer) {
        libvlc_time_t d = libvlc_media_player_get_length(m_vlcPlayer);
        return (d >= 0) ? d : 0;
    }
    return 0;
}

MediaBackend::State MediaBackend::state() {
    if (!m_vlcPlayer) return StoppedState;
    libvlc_state_t s = libvlc_media_player_get_state(m_vlcPlayer);
    switch (s) {
        case libvlc_NothingSpecial: return StoppedState;
        case libvlc_Opening: return StoppedState;
        case libvlc_Buffering: return PlayingState;
        case libvlc_Playing: return PlayingState;
        case libvlc_Paused: return PausedState;
        case libvlc_Stopped: return StoppedState;
        case libvlc_Ended: return EndOfMediaState;
        case libvlc_Error: return StoppedState;
    }
    return StoppedState;
}

QStringList MediaBackend::getOutputDevices() {
    QStringList list;
    if (!m_vlcPlayer) return list;

    m_audioOutputDevices.clear();

    // Add Default option
    AudioOutputDevice defaultDevice;
    defaultDevice.id = "";
    defaultDevice.name = "Default";
    m_audioOutputDevices.push_back(defaultDevice);
    list.append(defaultDevice.name);

    libvlc_media_player_t *tempPlayer = nullptr;
    libvlc_audio_output_device_t *devices = libvlc_audio_output_device_enum(m_vlcPlayer);
    if (!devices) {
        // Create a temporary player just to retrieve devices when the main player is stopped.
        // We must not release tempPlayer until after we copy all device descriptions.
        tempPlayer = libvlc_media_player_new(m_vlcInstance);
        if (tempPlayer) {
            devices = libvlc_audio_output_device_enum(tempPlayer);
        }
    }
    libvlc_audio_output_device_t *curr = devices;
    while (curr) {
        AudioOutputDevice dev;
        dev.id = QString::fromUtf8(curr->psz_device);
        dev.name = QString::fromUtf8(curr->psz_description);
        if (dev.name.isEmpty()) {
            dev.name = dev.id;
        }
        // Avoid duplicate names in list by appending ID if necessary, but description is usually fine
        m_audioOutputDevices.push_back(dev);
        list.append(dev.name);
        curr = curr->p_next;
    }
    m_logger->info("{} getOutputDevices() found {} devices:", m_loggingPrefix, m_audioOutputDevices.size());
    for (const auto &dev : m_audioOutputDevices) {
        m_logger->info("  Device ID: '{}' | Name: '{}'", dev.id.toStdString(), dev.name.toStdString());
    }

    if (devices) {
        libvlc_audio_output_device_list_release(devices);
    }
    if (tempPlayer) {
        libvlc_media_player_release(tempPlayer);
    }
    return list;
}

void MediaBackend::eventTimer_timeout() {
    if (!m_vlcPlayer) return;
    
    State curState = state();
    if (curState != m_state) {
        m_state = curState;
        emit stateChanged(m_state);
    }
    
    static qint64 lastDur = 0;
    qint64 dur = duration();
    if (dur != lastDur) {
        lastDur = dur;
        emit durationChanged(dur);
    }

    if (m_state == PlayingState) {
        emit positionChanged(position());

        if (m_silenceDetect && !m_silenceDetectedEmitted) {
            qint64 pos = position();
            if (dur > 10000 && pos > 10000) {
                qint64 remaining = dur - pos;
                if (remaining <= 4000) {
                    m_silenceDetectedEmitted = true;
                    m_logger->info("{} Simulating silence detection near end of track (remaining: {}ms)", m_loggingPrefix, remaining);
                    emit silenceDetected();
                }
            }
        }
    }
}

void MediaBackend::play() {
    if (m_vlcPlayer) {
        libvlc_media_player_play(m_vlcPlayer);
        m_state = PlayingState;
        emit stateChanged(m_state);
        if (m_fade) {
            m_fader->fadeIn(false);
        } else {
            m_fader->immediateIn();
        }
    }
}

void MediaBackend::pause() {
    if (m_vlcPlayer) {
        libvlc_media_player_pause(m_vlcPlayer);
        m_state = PausedState;
        emit stateChanged(m_state);
    }
}

void MediaBackend::setMedia(const QString &filename) {
    recreatePlayerIfNeeded();
    m_silenceDetectedEmitted = false;
    m_logger->info("{} Setting media: {}", m_loggingPrefix, filename.toStdString());
    rawStop();
    m_filename = filename;
    m_cdgFilename.clear();
    m_tempDir.reset();

    QString playPath = filename;

    if (filename.endsWith(".zip", Qt::CaseInsensitive)) {
        m_tempDir = std::make_unique<QTemporaryDir>();
        if (m_tempDir->isValid()) {
            MzArchive archive(filename);
            if (archive.isValidKaraokeFile()) {
                QString audioExt = archive.audioExtension();
                QString tempAudio = "temp_audio" + audioExt;
                QString tempCdg = "temp_audio.cdg";
                if (archive.extractAudio(m_tempDir->path(), tempAudio) &&
                    archive.extractCdg(m_tempDir->path(), tempCdg)) {
                    playPath = m_tempDir->path() + QDir::separator() + tempAudio;
                    m_cdgFilename = m_tempDir->path() + QDir::separator() + tempCdg;
                    m_logger->info("{} Successfully extracted zip archive to temp dir: {}", m_loggingPrefix, playPath.toStdString());
                } else {
                    m_logger->error("{} Failed to extract zip archive files", m_loggingPrefix);
                    emit audioError("Failed to extract zip archive");
                    return;
                }
            } else {
                m_logger->error("{} Invalid karaoke zip archive: {}", m_loggingPrefix, archive.getLastError().toStdString());
                emit audioError("Invalid zip archive: " + archive.getLastError());
                return;
            }
        } else {
            m_logger->error("{} Failed to create temporary directory", m_loggingPrefix);
            emit audioError("Failed to create temporary directory");
            return;
        }
    }

    if (m_vlcMedia) {
        libvlc_media_release(m_vlcMedia);
        m_vlcMedia = nullptr;
    }

    m_vlcMedia = libvlc_media_new_path(m_vlcInstance, QDir::toNativeSeparators(playPath).toLocal8Bit().constData());
    if (!m_vlcMedia) {
        m_logger->error("{} Failed to create LibVLC media for path: {}", m_loggingPrefix, playPath.toStdString());
        emit audioError("Failed to load media file");
        return;
    }

    libvlc_media_player_set_media(m_vlcPlayer, m_vlcMedia);
    m_hasVideo = false;
    emit hasActiveVideoChanged(false);

    updateVolume();
    updateAudioFilters();
}

void MediaBackend::setMediaCdg(const QString &cdgFilename, const QString &audioFilename) {
    recreatePlayerIfNeeded();
    m_silenceDetectedEmitted = false;
    m_logger->info("{} Setting media CDG: {} and Audio: {}", m_loggingPrefix, cdgFilename.toStdString(), audioFilename.toStdString());
    rawStop();
    m_filename = audioFilename;
    m_cdgFilename = cdgFilename;
    m_tempDir.reset();

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (m_tempDir->isValid()) {
        QFileInfo audioInfo(audioFilename);
        QString targetAudio = m_tempDir->path() + QDir::separator() + "song." + audioInfo.suffix();
        QString targetCdg = m_tempDir->path() + QDir::separator() + "song.cdg";

        if (QFile::copy(audioFilename, targetAudio) && QFile::copy(cdgFilename, targetCdg)) {
            if (m_vlcMedia) {
                libvlc_media_release(m_vlcMedia);
                m_vlcMedia = nullptr;
            }
            m_vlcMedia = libvlc_media_new_path(m_vlcInstance, QDir::toNativeSeparators(targetAudio).toLocal8Bit().constData());
            libvlc_media_player_set_media(m_vlcPlayer, m_vlcMedia);
            m_hasVideo = false;
            emit hasActiveVideoChanged(false);
            
            updateVolume();
            updateAudioFilters();
        } else {
            m_logger->error("{} Failed to copy media files to temp dir for playback", m_loggingPrefix);
            emit audioError("Failed to copy media files to temp dir");
        }
    } else {
        m_logger->error("{} Failed to create temporary directory", m_loggingPrefix);
        emit audioError("Failed to create temporary directory");
    }
}

void MediaBackend::setMuted(const bool &muted) {
    m_muted = muted;
    updateVolume();
    emit mutedChanged(muted);
}

bool MediaBackend::isMuted() {
    return m_muted;
}

void MediaBackend::setPosition(const qint64 &position) {
    if (m_vlcPlayer) {
        libvlc_media_player_set_time(m_vlcPlayer, position);
    }
}

void MediaBackend::setVolume(const int &volume) {
    m_volume = volume;
    updateVolume();
}

void MediaBackend::stop(const bool &skipFade) {
    if (m_fade && !skipFade && m_state == PlayingState) {
        m_fader->fadeOut(false);
    } else {
        m_fader->immediateOut();
        rawStop();
    }
}

void MediaBackend::rawStop() {
    if (m_vlcPlayer) {
        libvlc_media_player_stop(m_vlcPlayer);
    }
    m_state = StoppedState;
    emit stateChanged(m_state);
    for (auto* display : m_videoSinks) {
        if (display) {
            display->clearFrame();
        }
    }
}

void MediaBackend::setPitchShift(const int &pitchShift) {
    m_pitchShift = pitchShift;
    emit pitchChanged(m_pitchShift);
    m_logger->info("{} Pitch shift set to: {}", m_loggingPrefix, pitchShift);
}

void MediaBackend::fadeOut(const bool &waitForFade) {
    m_fader->fadeOut(waitForFade);
}

void MediaBackend::fadeIn(const bool &waitForFade) {
    m_fader->fadeIn(waitForFade);
}

void MediaBackend::setTempo(const int &percent) {
    m_tempoPercent = percent;
    if (m_vlcPlayer) {
        libvlc_media_player_set_rate(m_vlcPlayer, static_cast<float>(percent) / 100.0f);
    }
}

void MediaBackend::setMplxMode(const int &mode) {
    m_mplxMode = mode;
    updateAudioFilters();
}

void MediaBackend::setEqBypass(const bool &bypass) {
    m_eqBypass = bypass;
    updateAudioFilters();
}

void MediaBackend::setEqLevel(const int &band, const int &level) {
    if (band >= 0 && band < 10) {
        m_eqLevels[band] = level;
        updateAudioFilters();
    }
}

void MediaBackend::updateVolume() {
    if (!m_vlcPlayer) return;
    double volMult = (m_fader->state() == AudioFader::FadedIn) ? 1.0 :
                     (m_fader->state() == AudioFader::FadedOut) ? 0.0 :
                     (m_fader->state() == AudioFader::FadingIn || m_fader->state() == AudioFader::FadingOut) ? 0.5 : 1.0; 
    // Fallback multiplier check if not fully fading
    int vol = static_cast<int>(m_volume * volMult);
    if (m_muted) vol = 0;
    libvlc_audio_set_volume(m_vlcPlayer, vol);
    emit volumeChanged(m_volume);
}

void MediaBackend::updateAudioFilters() {
    if (!m_vlcPlayer) return;
    if (m_mplxMode == Multiplex_LeftChannel) {
        libvlc_audio_set_channel(m_vlcPlayer, libvlc_AudioChannel_Left);
    } else if (m_mplxMode == Multiplex_RightChannel) {
        libvlc_audio_set_channel(m_vlcPlayer, libvlc_AudioChannel_Right);
    } else {
        libvlc_audio_set_channel(m_vlcPlayer, libvlc_AudioChannel_Stereo);
    }
}

void MediaBackend::setDownmix(const bool &enabled) {
    if (state() == StoppedState) {
        recreatePlayerIfNeeded();
    }
}

void MediaBackend::recreatePlayerIfNeeded() {
    bool desiredMono = (m_type == Karaoke) ? m_settings.audioDownmix() : 
                       (m_type == BackgroundMusic) ? m_settings.audioDownmixBm() : false;

    if (desiredMono != m_isMonoInitialized) {
        m_logger->info("{} Recreating player to toggle mono downmix (desired: {})", m_loggingPrefix, desiredMono);

        rawStop();

        if (m_vlcPlayer) {
            libvlc_media_player_release(m_vlcPlayer);
            m_vlcPlayer = nullptr;
        }
        if (m_vlcInstance) {
            libvlc_release(m_vlcInstance);
            m_vlcInstance = nullptr;
        }

        std::vector<const char*> args;
        args.push_back("--no-video-title-show");
        args.push_back("--quiet");
        if (desiredMono) {
            args.push_back("--audio-filter=scaletempo,mono");
        } else {
            args.push_back("--audio-filter=scaletempo");
        }

        m_vlcInstance = libvlc_new(args.size(), args.data());
        if (!m_vlcInstance) {
            m_logger->error("{} Failed to recreate LibVLC instance", m_loggingPrefix);
            return;
        }
        m_vlcPlayer = libvlc_media_player_new(m_vlcInstance);
        if (!m_vlcPlayer) {
            m_logger->error("{} Failed to recreate LibVLC media player", m_loggingPrefix);
            return;
        }

        m_isMonoInitialized = desiredMono;
        setupVlcCallbacks();
        updateVolume();
        updateAudioFilters();

        // Restore output device
        if (m_type == Karaoke) {
            setAudioOutputDevice(m_settings.audioOutputDevice());
        } else if (m_type == BackgroundMusic) {
            setAudioOutputDevice(m_settings.audioOutputDeviceBm());
        }
    }
}
