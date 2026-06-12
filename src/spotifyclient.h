#ifndef SPOTIFYCLIENT_H
#define SPOTIFYCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "spotifyauthcontroller.h"
#include "settings.h"

class SpotifyClient : public QObject {
    Q_OBJECT
public:
    explicit SpotifyClient(SpotifyAuthController *auth, QObject *parent = nullptr);
    ~SpotifyClient() override;

    void setEnabled(bool enabled);

    // HTTP command controls
    void play();
    void pause();
    void next();
    void prev();
    void seek(int positionMs);
    void setVolume(int volumePercent);
    void loadUri(const QString &uri, bool play = true);
    void search(const QString &query);
    void fetchPlaylists();
    void fetchPlaylistTracks(const QString &playlistId);
    void fetchDevices();
    void transferPlayback(const QString &deviceId);

signals:
    // Player status signals (from polling)
    void playbackStateChanged(bool playing);
    void trackChanged(const QString &title, const QString &artist, const QString &album, const QString &coverUrl, int durationMs);
    void positionChanged(int positionMs);
    void volumeChanged(int volumePercent);
    
    // REST API response signals
    void searchResultsReceived(const QJsonArray &tracks);
    void playlistsReceived(const QJsonArray &playlists);
    void playlistTracksReceived(const QJsonArray &tracks);
    void devicesReceived(const QJsonArray &devices);

private slots:
    void pollPlayerState();

private:
    void sendPostRequest(const QString &path, const QByteArray &data = QByteArray(), const QString &contentType = "application/json");
    void sendPutRequest(const QString &path, const QByteArray &data, const QString &contentType = "application/json");
    void sendGetRequest(const QString &path, int requestType);

    SpotifyAuthController *m_auth{nullptr};
    QTimer *m_pollTimer{nullptr};
    std::shared_ptr<spdlog::logger> m_logger;
    QString m_loggingPrefix{"[SpotifyClient]"};
    Settings m_settings;
    
    bool m_wasPlaying{false};
    int m_lastPosition{-1};
    int m_lastVolume{-1};
    QString m_lastTrackId;

    enum RequestType {
        SearchRequest,
        PlaylistsRequest,
        PlaylistTracksRequest,
        DevicesRequest,
        PlayerStateRequest
    };
};

#endif // SPOTIFYCLIENT_H
