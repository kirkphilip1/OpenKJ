#include "spotifyauthcontroller.h"
#include "simplecrypt.h"
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>

SpotifyAuthController::SpotifyAuthController(QObject *parent)
    : QObject(parent),
      m_oauth(new QOAuth2AuthorizationCodeFlow(this)),
      m_replyHandler(new QOAuthHttpServerReplyHandler(8080, this)) // Local callback port
{
    m_logger = spdlog::get("logger");
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }

    // Set a QNetworkAccessManager eagerly to prevent null-pointer crashes on early requests
    m_oauth->setNetworkAccessManager(new QNetworkAccessManager(this));

    m_oauth->setReplyHandler(m_replyHandler);
    m_oauth->setAuthorizationUrl(QUrl("https://accounts.spotify.com/authorize"));
    m_oauth->setAccessTokenUrl(QUrl("https://accounts.spotify.com/api/token"));
    m_oauth->setClientIdentifier("65d22982423e4be7afdbaa3b5bade181");
    m_oauth->setClientIdentifierSharedKey("4154acfa3f514ba0bd4acc7ce72a6cb5");
    
    // Required scopes for typical Spotify integration
    m_oauth->setScope("user-read-playback-state user-modify-playback-state playlist-read-private user-library-read");

    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [=](const QUrl &url) {
        m_logger->info("[SpotifyAuth] Opening browser for authentication: {}", url.toString().toStdString());
        QDesktopServices::openUrl(url);
    });

    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::granted, this, &SpotifyAuthController::onGranted);
    
    // Save tokens whenever they change
    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::tokenChanged, this, &SpotifyAuthController::saveTokens);
    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::refreshTokenChanged, this, &SpotifyAuthController::saveTokens);

    // Attempt to load previously saved tokens
    loadTokens();
}

SpotifyAuthController::~SpotifyAuthController() {
}

void SpotifyAuthController::grant() {
    if (!m_isAuthenticated) {
        m_oauth->grant();
    }
}

void SpotifyAuthController::onGranted() {
    m_logger->info("[SpotifyAuth] Successfully authenticated with Spotify Web API.");
    m_isAuthenticated = true;
    emit authenticated();
    emit authStatusChanged(true);
}

void SpotifyAuthController::saveTokens() {
    QSettings settings("OpenKJ", "OpenKJ");
    SimpleCrypt crypto(m_cryptoKey);
    
    QString accessToken = m_oauth->token();
    QString refreshToken = m_oauth->refreshToken();
    
    if (!accessToken.isEmpty()) {
        settings.setValue("Spotify/AccessToken", crypto.encryptToString(accessToken));
    }
    if (!refreshToken.isEmpty()) {
        settings.setValue("Spotify/RefreshToken", crypto.encryptToString(refreshToken));
    }
}

void SpotifyAuthController::loadTokens() {
    QSettings settings("OpenKJ", "OpenKJ");
    SimpleCrypt crypto(m_cryptoKey);
    
    QString encAccessToken = settings.value("Spotify/AccessToken").toString();
    QString encRefreshToken = settings.value("Spotify/RefreshToken").toString();
    
    if (!encAccessToken.isEmpty() && !encRefreshToken.isEmpty()) {
        QString accessToken = crypto.decryptToString(encAccessToken);
        QString refreshToken = crypto.decryptToString(encRefreshToken);
        
        if (!accessToken.isEmpty() && !refreshToken.isEmpty()) {
            m_oauth->setToken(accessToken);
            m_oauth->setRefreshToken(refreshToken);
            
            m_logger->info("[SpotifyAuth] Loaded saved tokens, attempting to refresh/use...");
            
            // Note: In QtNetworkAuth, setting a token doesn't automatically mean we are valid if it's expired.
            // But we can trigger a refresh manually or the flow will handle it on next request.
            // For now, we trust the tokens and if a request fails with 401, QOAuth2AuthorizationCodeFlow can refresh.
            // In a robust implementation, we might validate the token right away.
        }
    }
}

QNetworkAccessManager* SpotifyAuthController::networkAccessManager() const {
    return m_oauth->networkAccessManager();
}

QOAuth2AuthorizationCodeFlow* SpotifyAuthController::oauthFlow() const {
    return m_oauth;
}

bool SpotifyAuthController::isAuthenticated() const {
    return m_isAuthenticated;
}
