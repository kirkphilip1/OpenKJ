#ifndef SPOTIFYTAB_H
#define SPOTIFYTAB_H

#include <QWidget>
#include <QDialog>
#include <QStackedWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QSlider>
#include <QSplitter>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMenu>
#include "spotifyauthcontroller.h"
#include "spotifyclient.h"

class SpotifyTab : public QWidget {
    Q_OBJECT
public:
    explicit SpotifyTab(SpotifyAuthController *auth, SpotifyClient *client, QWidget *parent = nullptr);
    ~SpotifyTab() override;

private slots:
    // Unconfigured view slots
    void onConnectClicked();
    void onAuthUrlReceived(const QString &url);
    void onAuthStatusChanged(bool authenticated);

    // Configured view slots
    void onPlayPauseClicked();
    void onPrevClicked();
    void onNextClicked();
    void onSearchClicked();
    void onSearchLineReturnPressed();
    void onPlaylistSelected(QListWidgetItem *item);
    void onTrackDoubleClicked(int row, int column);
    void onTrackContextMenu(const QPoint &pos);
    
    // Controls slots
    void onVolumeSliderChanged(int value);
    void onPositionSliderPressed();
    void onPositionSliderReleased();
    void onPositionSliderMoved(int value);
    
    // Options slots
    void onOptionsClicked();
    void onUseIntegratedPlayerToggled(bool checked);
    void onDeviceSelected(int index);

    // Client event slots
    void onPlaybackStateChanged(bool playing);
    void onTrackChanged(const QString &title, const QString &artist, const QString &album, const QString &coverUrl, int durationMs);
    void onPositionChanged(int positionMs);
    void onVolumeChanged(int volumePercent);
    void onSearchResultsReceived(const QJsonArray &tracks);
    void onPlaylistsReceived(const QJsonArray &playlists);
    void onPlaylistTracksReceived(const QJsonArray &tracks);
    void onDevicesReceived(const QJsonArray &devices);

    // Audio cover art loader
    void loadCoverArt(const QString &url);

private:
    void setupUI();
    void setupUnconfiguredView();
    void setupConfiguredView();
    void refreshDeviceList();

    SpotifyAuthController *m_auth;
    SpotifyClient *m_client;
    QNetworkAccessManager *m_coverArtLoader;

    // Stacked widget for views
    QStackedWidget *m_stackedWidget{nullptr};

    // Unconfigured view widgets
    QWidget *m_unconfiguredWidget{nullptr};
    QLabel *m_statusLabel{nullptr};
    QPushButton *m_connectButton{nullptr};

    // Configured view widgets
    QWidget *m_configuredWidget{nullptr};
    
    // Header
    QGroupBox *m_headerGroupBox{nullptr};
    QLabel *m_coverArtLabel{nullptr};
    QLabel *m_trackTitleLabel{nullptr};
    QLabel *m_trackArtistLabel{nullptr};
    QCheckBox *m_integratedPlayerCheckbox{nullptr};
    QComboBox *m_deviceComboBox{nullptr};
    QPushButton *m_optionsButton{nullptr};

    // Main Panels
    QSplitter *m_mainSplitter{nullptr};
    
    // Left panel (Playlists & Search)
    QWidget *m_leftPanel{nullptr};
    QLineEdit *m_searchEdit{nullptr};
    QPushButton *m_searchButton{nullptr};
    QListWidget *m_playlistsListWidget{nullptr};

    // Center panel (Track List)
    QTableWidget *m_tracksTableWidget{nullptr};

    // Right panel (Queue)
    QWidget *m_rightPanel{nullptr};
    QListWidget *m_queueListWidget{nullptr};

    // Footer Player Bar
    QWidget *m_footerWidget{nullptr};
    QPushButton *m_playPauseButton{nullptr};
    QPushButton *m_prevButton{nullptr};
    QPushButton *m_nextButton{nullptr};
    QSlider *m_positionSlider{nullptr};
    QLabel *m_currentTimeLabel{nullptr};
    QLabel *m_totalTimeLabel{nullptr};
    QSlider *m_volumeSlider{nullptr};
    QLabel *m_volumeLabel{nullptr};

    // State tracking variables
    bool m_isPlaying{false};
    bool m_isSliderPressed{false};
    int m_trackDurationMs{0};
    QMap<QString, QString> m_playlistIds; // playlist name -> ID
    QMap<QString, QString> m_deviceIds;   // device name -> ID
    QJsonArray m_currentTracksList;        // currently shown tracks in table
    QList<QJsonObject> m_localQueue;       // tracks currently in play queue
};

// Spotify settings Dialog
class SpotifyOptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpotifyOptionsDialog(QWidget *parent = nullptr);
    bool useForBreakMusic() const;
    void setUseForBreakMusic(bool enabled);

private:
    QCheckBox *m_breakMusicCheckbox;
};

#endif // SPOTIFYTAB_H
