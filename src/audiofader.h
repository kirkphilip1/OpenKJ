#ifndef AUDIOFADER_H
#define AUDIOFADER_H

#include <QObject>
#include <QTimer>
#include <spdlog/async_logger.h>

class AudioFader : public QObject
{
    Q_OBJECT
public:
    explicit AudioFader(QObject *parent = nullptr);
    enum FaderState{FadedIn=0,FadingIn,FadedOut,FadingOut};
    [[nodiscard]] static std::string stateToStr(FaderState state);
    void setVolumeElement(void *volumeElement) {}
    void setObjName(const QString &name);
    [[nodiscard]] bool isFading();
    void setVolume(double volume);
    void immediateIn();
    void immediateOut();
    [[nodiscard]] FaderState state();

private:
    double volume();

    double m_volume{1.0};
    QTimer m_timer;
    double m_targetVol{0.0};
    QString m_objName;
    std::shared_ptr<spdlog::logger> m_logger;
    FaderState m_curState{FadedIn};

signals:
    void volumeChanged(double volume);
    void fadeStarted();
    void fadeComplete();
    void faderStateChanged(AudioFader::FaderState);

public slots:
    void fadeOut(bool block = false);
    void fadeIn(bool block = false);

private slots:
    void timerTimeout();
};

#endif // AUDIOFADER_H
