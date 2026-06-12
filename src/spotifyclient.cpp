#include "spotifyclient.h"
#include <QUrlQuery>

SpotifyClient::SpotifyClient(SpotifyAuthController *auth, QObject *parent) 
    : QObject(parent), m_auth(auth) 
{
    m_logger = spdlog::get("logger");
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &SpotifyClient::pollPlayerState);
    
    connect(m_auth, &SpotifyAuthController::authStatusChanged, this, [=](bool authOK) {
        if (authOK && m_settings.spotifyEnabled()) {
            m_pollTimer->start(2000); // Poll every 2 seconds
        } else {
            m_pollTimer->stop();
        }
    });

    if (m_auth->isAuthenticated() && m_settings.spotifyEnabled()) {
        m_pollTimer->start(2000);
    }
}

SpotifyClient::~SpotifyClient() {
}

void SpotifyClient::setEnabled(bool enabled) {
    if (enabled) {
        if (m_auth->isAuthenticated() && !m_pollTimer->isActive()) {
            m_pollTimer->start(2000);
        }
    } else {
        m_pollTimer->stop();
    }
}

void SpotifyClient::pollPlayerState() {
    if (!m_settings.spotifyEnabled()) {
        m_pollTimer->stop();
        return;
    }
    sendGetRequest("/v1/me/player", PlayerStateRequest);
}

void SpotifyClient::sendPostRequest(const QString &path, const QByteArray &data, const QString &contentType) {
    if (!m_auth || !m_auth->oauthFlow() || !m_auth->networkAccessManager()) {
        m_logger->error("{} Cannot send POST request, Spotify Auth or Network Access Manager is null.", m_loggingPrefix.toStdString());
        return;
    }
    QNetworkRequest request(QUrl("https://api.spotify.com" + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    request.setRawHeader("Authorization", "Bearer " + m_auth->oauthFlow()->token().toUtf8());
    QNetworkReply *reply = m_auth->networkAccessManager()->post(request, data);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void SpotifyClient::sendPutRequest(const QString &path, const QByteArray &data, const QString &contentType) {
    if (!m_auth || !m_auth->oauthFlow() || !m_auth->networkAccessManager()) {
        m_logger->error("{} Cannot send PUT request, Spotify Auth or Network Access Manager is null.", m_loggingPrefix.toStdString());
        return;
    }
    QNetworkRequest request(QUrl("https://api.spotify.com" + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    request.setRawHeader("Authorization", "Bearer " + m_auth->oauthFlow()->token().toUtf8());
    QNetworkReply *reply = m_auth->networkAccessManager()->put(request, data);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void SpotifyClient::sendGetRequest(const QString &path, int requestType) {
    if (!m_auth || !m_auth->oauthFlow() || !m_auth->oauthFlow()->networkAccessManager()) {
        m_logger->error("{} Cannot send GET request, Spotify Auth or Network Access Manager is null.", m_loggingPrefix.toStdString());
        return;
    }
    QNetworkReply *reply = m_auth->oauthFlow()->get(QUrl("https://api.spotify.com" + path));
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            if (data.isEmpty()) {
                reply->deleteLater();
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (requestType == SearchRequest) {
                    QJsonObject tracksObj = obj.value("tracks").toObject();
                    emit searchResultsReceived(tracksObj.value("items").toArray());
                } 
                else if (requestType == PlaylistsRequest) {
                    emit playlistsReceived(obj.value("items").toArray());
                } 
                else if (requestType == PlaylistTracksRequest) {
                    emit playlistTracksReceived(obj.value("items").toArray());
                }
                else if (requestType == DevicesRequest) {
                    emit devicesReceived(obj.value("devices").toArray());
                }
                else if (requestType == PlayerStateRequest) {
                    bool isPlaying = obj.value("is_playing").toBool();
                    int progress = obj.value("progress_ms").toInt();
                    
                    QJsonObject device = obj.value("device").toObject();
                    int volume = device.value("volume_percent").toInt();
                    
                    QJsonObject item = obj.value("item").toObject();
                    QString trackId = item.value("id").toString();
                    
                    if (isPlaying != m_wasPlaying) {
                        m_wasPlaying = isPlaying;
                        emit playbackStateChanged(isPlaying);
                    }
                    if (qAbs(progress - m_lastPosition) > 2500) { // Only emit significant seek changes or regular updates
                        m_lastPosition = progress;
                        emit positionChanged(progress);
                    }
                    if (volume != m_lastVolume) {
                        m_lastVolume = volume;
                        emit volumeChanged(volume);
                    }
                    if (trackId != m_lastTrackId && !trackId.isEmpty()) {
                        m_lastTrackId = trackId;
                        QString title = item.value("name").toString();
                        int duration = item.value("duration_ms").toInt();
                        
                        QJsonArray artists = item.value("artists").toArray();
                        QStringList artistList;
                        for (const auto &art : artists) {
                            artistList << art.toObject().value("name").toString();
                        }
                        QString artist = artistList.join(", ");
                        
                        QJsonObject albumObj = item.value("album").toObject();
                        QString album = albumObj.value("name").toString();
                        
                        QJsonArray images = albumObj.value("images").toArray();
                        QString coverUrl;
                        if (!images.isEmpty()) {
                            coverUrl = images[0].toObject().value("url").toString();
                        }
                        
                        emit trackChanged(title, artist, album, coverUrl, duration);
                    }
                }
            }
        } else {
            // Ignore 204 No Content for PlayerStateRequest when no active device
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 204) {
                m_logger->error("{} HTTP GET request to {} failed: {}", m_loggingPrefix.toStdString(), path.toStdString(), reply->errorString().toStdString());
            }
        }
        reply->deleteLater();
    });
}

void SpotifyClient::play() {
    sendPutRequest("/v1/me/player/play", QByteArray());
}

void SpotifyClient::pause() {
    sendPutRequest("/v1/me/player/pause", QByteArray());
}

void SpotifyClient::next() {
    sendPostRequest("/v1/me/player/next");
}

void SpotifyClient::prev() {
    sendPostRequest("/v1/me/player/previous");
}

void SpotifyClient::seek(int positionMs) {
    sendPutRequest("/v1/me/player/seek?position_ms=" + QString::number(positionMs).toUtf8(), QByteArray());
}

void SpotifyClient::setVolume(int volumePercent) {
    sendPutRequest("/v1/me/player/volume?volume_percent=" + QString::number(volumePercent).toUtf8(), QByteArray());
}

void SpotifyClient::loadUri(const QString &uri, bool play) {
    QJsonObject body;
    QJsonArray uris;
    uris.append(uri);
    
    if (uri.contains("track")) {
        body["uris"] = uris;
    } else {
        body["context_uri"] = uri;
    }
    
    sendPutRequest("/v1/me/player/play", QJsonDocument(body).toJson());
}

void SpotifyClient::search(const QString &query) {
    QString path = "/v1/search?q=" + QUrl::toPercentEncoding(query) + "&type=track&limit=50";
    sendGetRequest(path, SearchRequest);
}

void SpotifyClient::fetchPlaylists() {
    sendGetRequest("/v1/me/playlists?limit=50", PlaylistsRequest);
}

void SpotifyClient::fetchPlaylistTracks(const QString &playlistId) {
    sendGetRequest("/v1/playlists/" + playlistId + "/tracks?limit=100", PlaylistTracksRequest);
}

void SpotifyClient::fetchDevices() {
    sendGetRequest("/v1/me/player/devices", DevicesRequest);
}

void SpotifyClient::transferPlayback(const QString &deviceId) {
    QJsonObject body;
    QJsonArray devices;
    devices.append(deviceId);
    body["device_ids"] = devices;
    body["play"] = true;
    sendPutRequest("/v1/me/player", QJsonDocument(body).toJson());
}
