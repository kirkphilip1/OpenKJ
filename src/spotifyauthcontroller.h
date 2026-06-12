#ifndef SPOTIFYAUTHCONTROLLER_H
#define SPOTIFYAUTHCONTROLLER_H

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QNetworkAccessManager>
#include <spdlog/spdlog.h>

class SpotifyAuthController : public QObject {
    Q_OBJECT
public:
    explicit SpotifyAuthController(QObject *parent = nullptr);
    ~SpotifyAuthController() override;

    void grant();
    
    QNetworkAccessManager* networkAccessManager() const;
    QOAuth2AuthorizationCodeFlow* oauthFlow() const;
    bool isAuthenticated() const;

signals:
    void authenticated();
    void error(const QString& errorMessage);
    void authStatusChanged(bool isAuthenticated);

private slots:
    void onGranted();
    void saveTokens();
    void loadTokens();

private:
    QOAuth2AuthorizationCodeFlow *m_oauth{nullptr};
    QOAuthHttpServerReplyHandler *m_replyHandler{nullptr};
    std::shared_ptr<spdlog::logger> m_logger;
    bool m_isAuthenticated{false};
    
    // Simple encryption key for storing tokens in QSettings
    quint64 m_cryptoKey{0x0c2ad4a4acb9f022}; 
};

#endif // SPOTIFYAUTHCONTROLLER_H
