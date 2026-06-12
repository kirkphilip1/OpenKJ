#include "spotifytab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// SpotifyOptionsDialog implementation
SpotifyOptionsDialog::SpotifyOptionsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Spotify Options"));
    setMinimumWidth(350);

    auto *layout = new QVBoxLayout(this);
    m_breakMusicCheckbox = new QCheckBox(tr("Use spotify for break music between karaoke tracks"), this);
    m_breakMusicCheckbox->setChecked(useForBreakMusic());
    m_breakMusicCheckbox->setStyleSheet("font-size: 13px; padding: 10px; color: #FFFFFF;");
    layout->addWidget(m_breakMusicCheckbox);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [=]() {
        setUseForBreakMusic(m_breakMusicCheckbox->isChecked());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    setStyleSheet("background-color: #181818; color: white;");
}

bool SpotifyOptionsDialog::useForBreakMusic() const {
    QSettings settings;
    return settings.value("spotify/use_for_break_music", false).toBool();
}

void SpotifyOptionsDialog::setUseForBreakMusic(bool enabled) {
    QSettings settings;
    settings.setValue("spotify/use_for_break_music", enabled);
}


// SpotifyTab implementation
SpotifyTab::SpotifyTab(SpotifyAuthController *auth, SpotifyClient *client, QWidget *parent)
    : QWidget(parent), m_auth(auth), m_client(client) 
{
    m_coverArtLoader = new QNetworkAccessManager(this);

    // Apply Spotify Sleek Dark styling
    setStyleSheet(
        "QWidget {"
        "    background-color: #121212;"
        "    color: #E0E0E0;"
        "    font-family: 'Segoe UI', 'Inter', sans-serif;"
        "}"
        "QPushButton {"
        "    background-color: #1DB954;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 15px;"
        "    padding: 6px 15px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1ED760;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1aa34a;"
        "}"
        "QLineEdit {"
        "    background-color: #242424;"
        "    border: 1px solid #3e3e3e;"
        "    border-radius: 12px;"
        "    padding: 5px 12px;"
        "    color: white;"
        "}"
        "QTableWidget {"
        "    background-color: #181818;"
        "    gridline-color: #282828;"
        "    border: none;"
        "    alternate-background-color: #1a1a1a;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #282828;"
        "    color: #1DB954;"
        "}"
        "QListWidget {"
        "    background-color: #181818;"
        "    border: none;"
        "    font-size: 13px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #282828;"
        "    color: #1DB954;"
        "}"
        "QGroupBox {"
        "    border: 1px solid #282828;"
        "    border-radius: 8px;"
        "    margin-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 3px 0 3px;"
        "}"
        "QSlider::groove:horizontal {"
        "    height: 4px;"
        "    background: #404040;"
        "    border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: #1DB954;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #ffffff;"
        "    width: 12px;"
        "    height: 12px;"
        "    margin: -4px 0;"
        "    border-radius: 6px;"
        "}"
    );

    setupUI();

    // Connect auth state signals
    connect(m_auth, &SpotifyAuthController::authStatusChanged, this, &SpotifyTab::onAuthStatusChanged);

    // Connect client WebSocket and API signals
    connect(m_client, &SpotifyClient::playbackStateChanged, this, &SpotifyTab::onPlaybackStateChanged);
    connect(m_client, &SpotifyClient::trackChanged, this, &SpotifyTab::onTrackChanged);
    connect(m_client, &SpotifyClient::positionChanged, this, &SpotifyTab::onPositionChanged);
    connect(m_client, &SpotifyClient::volumeChanged, this, &SpotifyTab::onVolumeChanged);
    
    connect(m_client, &SpotifyClient::searchResultsReceived, this, &SpotifyTab::onSearchResultsReceived);
    connect(m_client, &SpotifyClient::playlistsReceived, this, &SpotifyTab::onPlaylistsReceived);
    connect(m_client, &SpotifyClient::playlistTracksReceived, this, &SpotifyTab::onPlaylistTracksReceived);
    connect(m_client, &SpotifyClient::devicesReceived, this, &SpotifyTab::onDevicesReceived);

    // Initial configuration check
    onAuthStatusChanged(m_auth->isAuthenticated());
}

SpotifyTab::~SpotifyTab() {
}

void SpotifyTab::setupUI() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    setupUnconfiguredView();
    setupConfiguredView();

    m_stackedWidget->addWidget(m_unconfiguredWidget);
    m_stackedWidget->addWidget(m_configuredWidget);
}

void SpotifyTab::setupUnconfiguredView() {
    m_unconfiguredWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(m_unconfiguredWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    auto *logoLabel = new QLabel(this);
    logoLabel->setText("<h1 style='color:#1DB954; font-size:48px; font-weight:bold; margin:0;'>Spotify</h1>");
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);

    auto *infoLabel = new QLabel(this);
    infoLabel->setText(tr("Connect your Spotify Premium account to use Spotify for break music."));
    infoLabel->setStyleSheet("font-size: 15px; color: #B3B3B3;");
    infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(infoLabel);

    m_connectButton = new QPushButton(tr("Connect Spotify Account"), this);
    m_connectButton->setMinimumSize(220, 40);
    connect(m_connectButton, &QPushButton::clicked, this, &SpotifyTab::onConnectClicked);
    layout->addWidget(m_connectButton, 0, Qt::AlignCenter);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("font-size: 13px; color: #808080;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);
}

void SpotifyTab::setupConfiguredView() {
    m_configuredWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(m_configuredWidget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // 1. Header Player Details
    m_headerGroupBox = new QGroupBox(tr("Active Spotify Client"), this);
    auto *headerLayout = new QHBoxLayout(m_headerGroupBox);
    headerLayout->setContentsMargins(10, 8, 10, 8);
    headerLayout->setSpacing(15);

    // Album Art Preview
    m_coverArtLabel = new QLabel(m_headerGroupBox);
    m_coverArtLabel->setFixedSize(50, 50);
    m_coverArtLabel->setStyleSheet("background-color: #282828; border-radius: 4px;");
    m_coverArtLabel->setAlignment(Qt::AlignCenter);
    m_coverArtLabel->setText(tr("♫"));
    headerLayout->addWidget(m_coverArtLabel);

    // Title / Artist Metadata
    auto *metaLayout = new QVBoxLayout();
    metaLayout->setSpacing(2);
    m_trackTitleLabel = new QLabel(tr("Not Playing"), m_headerGroupBox);
    m_trackTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FFFFFF;");
    m_trackArtistLabel = new QLabel(tr("Unknown Artist"), m_headerGroupBox);
    m_trackArtistLabel->setStyleSheet("font-size: 12px; color: #B3B3B3;");
    metaLayout->addWidget(m_trackTitleLabel);
    metaLayout->addWidget(m_trackArtistLabel);
    headerLayout->addLayout(metaLayout);

    headerLayout->addStretch();

    // Active Device Dropdown
    m_deviceComboBox = new QComboBox(m_headerGroupBox);
    m_deviceComboBox->setMinimumWidth(150);
    connect(m_deviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpotifyTab::onDeviceSelected);
    headerLayout->addWidget(m_deviceComboBox);

    // Options Button
    m_optionsButton = new QPushButton(tr("Options"), m_headerGroupBox);
    m_optionsButton->setStyleSheet("background-color: #282828; color: white; border-radius: 12px; padding: 4px 12px;");
    connect(m_optionsButton, &QPushButton::clicked, this, &SpotifyTab::onOptionsClicked);
    headerLayout->addWidget(m_optionsButton);

    layout->addWidget(m_headerGroupBox);

    // 2. Main Area Splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_configuredWidget);
    layout->addWidget(m_mainSplitter);

    // Left Panel (Search & Playlists)
    m_leftPanel = new QWidget(m_mainSplitter);
    auto *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(m_leftPanel);
    m_searchEdit->setPlaceholderText(tr("Search Spotify..."));
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SpotifyTab::onSearchLineReturnPressed);
    m_searchButton = new QPushButton(tr("Search"), m_leftPanel);
    m_searchButton->setMinimumHeight(24);
    connect(m_searchButton, &QPushButton::clicked, this, &SpotifyTab::onSearchClicked);
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_searchButton);
    leftLayout->addLayout(searchLayout);

    m_playlistsListWidget = new QListWidget(m_leftPanel);
    connect(m_playlistsListWidget, &QListWidget::itemClicked, this, &SpotifyTab::onPlaylistSelected);
    leftLayout->addWidget(m_playlistsListWidget);

    m_mainSplitter->addWidget(m_leftPanel);

    // Center Panel (Track list)
    m_tracksTableWidget = new QTableWidget(m_mainSplitter);
    m_tracksTableWidget->setColumnCount(4);
    m_tracksTableWidget->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Album"), tr("Duration")});
    m_tracksTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tracksTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tracksTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tracksTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tracksTableWidget, &QTableWidget::cellDoubleClicked, this, &SpotifyTab::onTrackDoubleClicked);
    connect(m_tracksTableWidget, &QTableWidget::customContextMenuRequested, this, &SpotifyTab::onTrackContextMenu);
    m_mainSplitter->addWidget(m_tracksTableWidget);

    // Right Panel (Queue)
    m_rightPanel = new QWidget(m_mainSplitter);
    auto *rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    auto *queueHeader = new QLabel(tr("<b>Spotify Play Queue</b>"), m_rightPanel);
    queueHeader->setStyleSheet("font-size: 13px; color: #FFFFFF; padding-bottom: 4px;");
    rightLayout->addWidget(queueHeader);

    m_queueListWidget = new QListWidget(m_rightPanel);
    rightLayout->addWidget(m_queueListWidget);
    m_mainSplitter->addWidget(m_rightPanel);

    // Set Splitter sizing
    m_mainSplitter->setSizes({180, 450, 180});

    // 3. Footer Player Bar
    m_footerWidget = new QWidget(m_configuredWidget);
    m_footerWidget->setFixedHeight(55);
    auto *footerLayout = new QHBoxLayout(m_footerWidget);
    footerLayout->setContentsMargins(10, 4, 10, 4);

    // Player Controls
    m_prevButton = new QPushButton(tr("⏮"), m_footerWidget);
    m_prevButton->setFixedSize(30, 30);
    m_prevButton->setStyleSheet("background-color: #282828; border-radius: 15px; font-size: 14px;");
    connect(m_prevButton, &QPushButton::clicked, this, &SpotifyTab::onPrevClicked);
    footerLayout->addWidget(m_prevButton);

    m_playPauseButton = new QPushButton(tr("▶"), m_footerWidget);
    m_playPauseButton->setFixedSize(36, 36);
    m_playPauseButton->setStyleSheet("background-color: #FFFFFF; color: #121212; border-radius: 18px; font-size: 16px; font-weight: bold;");
    connect(m_playPauseButton, &QPushButton::clicked, this, &SpotifyTab::onPlayPauseClicked);
    footerLayout->addWidget(m_playPauseButton);

    m_nextButton = new QPushButton(tr("⏭"), m_footerWidget);
    m_nextButton->setFixedSize(30, 30);
    m_nextButton->setStyleSheet("background-color: #282828; border-radius: 15px; font-size: 14px;");
    connect(m_nextButton, &QPushButton::clicked, this, &SpotifyTab::onNextClicked);
    footerLayout->addWidget(m_nextButton);

    // Scrubber / Position Slider
    m_currentTimeLabel = new QLabel("0:00", m_footerWidget);
    m_currentTimeLabel->setStyleSheet("font-size: 11px; color: #B3B3B3;");
    footerLayout->addWidget(m_currentTimeLabel);

    m_positionSlider = new QSlider(Qt::Horizontal, m_footerWidget);
    m_positionSlider->setRange(0, 1000);
    connect(m_positionSlider, &QSlider::sliderPressed, this, &SpotifyTab::onPositionSliderPressed);
    connect(m_positionSlider, &QSlider::sliderReleased, this, &SpotifyTab::onPositionSliderReleased);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &SpotifyTab::onPositionSliderMoved);
    footerLayout->addWidget(m_positionSlider);

    m_totalTimeLabel = new QLabel("0:00", m_footerWidget);
    m_totalTimeLabel->setStyleSheet("font-size: 11px; color: #B3B3B3;");
    footerLayout->addWidget(m_totalTimeLabel);

    // Volume Control
    auto *volIcon = new QLabel("🔊", m_footerWidget);
    volIcon->setStyleSheet("font-size: 13px; margin-left: 15px;");
    footerLayout->addWidget(volIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal, m_footerWidget);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(75);
    m_volumeSlider->setFixedWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &SpotifyTab::onVolumeSliderChanged);
    footerLayout->addWidget(m_volumeSlider);

    m_volumeLabel = new QLabel("75%", m_footerWidget);
    m_volumeLabel->setStyleSheet("font-size: 11px; color: #B3B3B3; min-width: 30px;");
    footerLayout->addWidget(m_volumeLabel);

    layout->addWidget(m_footerWidget);
}

// Unconfigured slots
void SpotifyTab::onConnectClicked() {
    m_statusLabel->setText(tr("Opening browser for Spotify authentication..."));
    m_auth->grant();
}

void SpotifyTab::onAuthUrlReceived(const QString &url) {
    Q_UNUSED(url);
}

void SpotifyTab::onAuthStatusChanged(bool authenticated) {
    if (authenticated) {
        m_stackedWidget->setCurrentIndex(1); // Show configured
        
        // Fetch initial playlists and active devices
        QTimer::singleShot(2000, this, [=]() {
            m_client->fetchPlaylists();
            m_client->fetchDevices();
        });
    } else {
        m_stackedWidget->setCurrentIndex(0); // Show unconfigured instructions
    }
}

// Configured slots
void SpotifyTab::onPlayPauseClicked() {
    if (m_isPlaying) {
        m_client->pause();
    } else {
        m_client->play();
    }
}

void SpotifyTab::onPrevClicked() {
    m_client->prev();
}

void SpotifyTab::onNextClicked() {
    m_client->next();
}

void SpotifyTab::onSearchClicked() {
    QString query = m_searchEdit->text().trimmed();
    if (!query.isEmpty()) {
        m_client->search(query);
    }
}

void SpotifyTab::onSearchLineReturnPressed() {
    onSearchClicked();
}

void SpotifyTab::onPlaylistSelected(QListWidgetItem *item) {
    if (!item) return;
    QString playlistName = item->text();
    QString id = m_playlistIds.value(playlistName);
    if (!id.isEmpty()) {
        m_client->fetchPlaylistTracks(id);
    }
}

void SpotifyTab::onTrackDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row < 0 || row >= m_currentTracksList.size()) return;

    QJsonObject track = m_currentTracksList.at(row).toObject();
    // Sometimes tracks list comes from playlist details where it's wrapped in a "track" object
    if (track.contains("track")) {
        track = track.value("track").toObject();
    }
    
    QString uri = track.value("uri").toString();
    if (!uri.isEmpty()) {
        m_client->loadUri(uri, true);
    }
}

void SpotifyTab::onTrackContextMenu(const QPoint &pos) {
    QTableWidgetItem *item = m_tracksTableWidget->itemAt(pos);
    if (!item) return;
    
    int row = item->row();
    if (row < 0 || row >= m_currentTracksList.size()) return;

    QJsonObject track = m_currentTracksList.at(row).toObject();
    if (track.contains("track")) {
        track = track.value("track").toObject();
    }

    QMenu menu(this);
    QAction *playAction = menu.addAction(tr("Play Now"));
    QAction *queueAction = menu.addAction(tr("Add to Queue"));

    QAction *selected = menu.exec(m_tracksTableWidget->viewport()->mapToGlobal(pos));
    if (!selected) return;

    QString uri = track.value("uri").toString();
    if (uri.isEmpty()) return;

    if (selected == playAction) {
        m_client->loadUri(uri, true);
    } else if (selected == queueAction) {
        // Send to Spotify API queue
        QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player/queue?uri=" + QUrl::toPercentEncoding(uri)));
        m_auth->networkAccessManager()->post(req, QByteArray());

        // Update local queue list widget
        QString title = track.value("name").toString();
        
        QJsonArray artists = track.value("artists").toArray();
        if (artists.isEmpty()) {
            artists = track.value("artist_names").toArray();
        }
        QStringList artNames;
        for (const auto &a : artists) {
            if (a.isObject()) artNames << a.toObject().value("name").toString();
            else artNames << a.toString();
        }
        QString artist = artNames.join(", ");

        m_queueListWidget->addItem(title + " - " + artist);
        m_localQueue.append(track);
    }
}

// Scrubber slots
void SpotifyTab::onVolumeSliderChanged(int value) {
    m_volumeLabel->setText(QString::number(value) + "%");
    m_client->setVolume(value);
}

void SpotifyTab::onPositionSliderPressed() {
    m_isSliderPressed = true;
}

void SpotifyTab::onPositionSliderReleased() {
    m_isSliderPressed = false;
    int value = m_positionSlider->value();
    int targetMs = (value * m_trackDurationMs) / 1000;
    m_client->seek(targetMs);
}

void SpotifyTab::onPositionSliderMoved(int value) {
    if (m_trackDurationMs > 0) {
        int currentMs = (value * m_trackDurationMs) / 1000;
        int seconds = (currentMs / 1000) % 60;
        int minutes = (currentMs / (1000 * 60));
        m_currentTimeLabel->setText(QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
    }
}

// Options dialog trigger
void SpotifyTab::onOptionsClicked() {
    SpotifyOptionsDialog dialog(this);
    dialog.exec();
}

void SpotifyTab::onUseIntegratedPlayerToggled(bool checked) {
    Q_UNUSED(checked);
}

void SpotifyTab::onDeviceSelected(int index) {
    if (index < 0) return;
    
    QString name = m_deviceComboBox->itemText(index);
    QString id = m_deviceIds.value(name);
    if (!id.isEmpty()) {
        m_client->transferPlayback(id);
    }
}

// Client event slots
void SpotifyTab::onPlaybackStateChanged(bool playing) {
    m_isPlaying = playing;
    m_playPauseButton->setText(playing ? "⏸" : "▶");
}

void SpotifyTab::onTrackChanged(const QString &title, const QString &artist, const QString &album, const QString &coverUrl, int durationMs) {
    m_trackTitleLabel->setText(title.isEmpty() ? tr("Not Playing") : title);
    m_trackArtistLabel->setText(artist.isEmpty() ? tr("Unknown Artist") : artist);
    m_trackDurationMs = durationMs;

    int seconds = (durationMs / 1000) % 60;
    int minutes = (durationMs / (1000 * 60));
    m_totalTimeLabel->setText(QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
    m_currentTimeLabel->setText("0:00");
    m_positionSlider->setValue(0);

    // Clear top element from queue if it matches what started playing
    if (!m_localQueue.isEmpty()) {
        QJsonObject nextTrack = m_localQueue.first();
        if (nextTrack.value("name").toString() == title) {
            m_localQueue.removeFirst();
            delete m_queueListWidget->takeItem(0);
        }
    }

    loadCoverArt(coverUrl);
}

void SpotifyTab::onPositionChanged(int positionMs) {
    if (m_isSliderPressed || m_trackDurationMs <= 0) return;

    int progress = (positionMs * 1000) / m_trackDurationMs;
    m_positionSlider->setValue(progress);

    int seconds = (positionMs / 1000) % 60;
    int minutes = (positionMs / (1000 * 60));
    m_currentTimeLabel->setText(QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
}

void SpotifyTab::onVolumeChanged(int volumePercent) {
    m_volumeSlider->setValue(volumePercent);
    m_volumeLabel->setText(QString::number(volumePercent) + "%");
}

void SpotifyTab::onSearchResultsReceived(const QJsonArray &tracks) {
    m_currentTracksList = tracks;
    m_tracksTableWidget->setRowCount(0);
    m_tracksTableWidget->setRowCount(tracks.size());

    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject track = tracks.at(i).toObject();
        
        QString title = track.value("name").toString();
        
        QJsonArray artists = track.value("artists").toArray();
        QStringList artNames;
        for (const auto &a : artists) {
            artNames << a.toObject().value("name").toString();
        }
        QString artist = artNames.join(", ");

        QJsonObject albumObj = track.value("album").toObject();
        QString album = albumObj.value("name").toString();

        int durMs = track.value("duration_ms").toInt();
        int sec = (durMs / 1000) % 60;
        int min = (durMs / (1000 * 60));
        QString duration = QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));

        m_tracksTableWidget->setItem(i, 0, new QTableWidgetItem(title));
        m_tracksTableWidget->setItem(i, 1, new QTableWidgetItem(artist));
        m_tracksTableWidget->setItem(i, 2, new QTableWidgetItem(album));
        m_tracksTableWidget->setItem(i, 3, new QTableWidgetItem(duration));
    }
}

void SpotifyTab::onPlaylistsReceived(const QJsonArray &playlists) {
    m_playlistsListWidget->clear();
    m_playlistIds.clear();

    for (const auto &plValue : playlists) {
        QJsonObject pl = plValue.toObject();
        QString name = pl.value("name").toString();
        QString id = pl.value("id").toString();
        if (!name.isEmpty() && !id.isEmpty()) {
            m_playlistsListWidget->addItem(name);
            m_playlistIds.insert(name, id);
        }
    }
}

void SpotifyTab::onPlaylistTracksReceived(const QJsonArray &tracks) {
    m_currentTracksList = tracks;
    m_tracksTableWidget->setRowCount(0);
    m_tracksTableWidget->setRowCount(tracks.size());

    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject trackContainer = tracks.at(i).toObject();
        QJsonObject track = trackContainer.value("track").toObject();
        
        QString title = track.value("name").toString();
        
        QJsonArray artists = track.value("artists").toArray();
        QStringList artNames;
        for (const auto &a : artists) {
            artNames << a.toObject().value("name").toString();
        }
        QString artist = artNames.join(", ");

        QJsonObject albumObj = track.value("album").toObject();
        QString album = albumObj.value("name").toString();

        int durMs = track.value("duration_ms").toInt();
        int sec = (durMs / 1000) % 60;
        int min = (durMs / (1000 * 60));
        QString duration = QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));

        m_tracksTableWidget->setItem(i, 0, new QTableWidgetItem(title));
        m_tracksTableWidget->setItem(i, 1, new QTableWidgetItem(artist));
        m_tracksTableWidget->setItem(i, 2, new QTableWidgetItem(album));
        m_tracksTableWidget->setItem(i, 3, new QTableWidgetItem(duration));
    }
}

void SpotifyTab::onDevicesReceived(const QJsonArray &devices) {
    m_deviceComboBox->clear();
    m_deviceIds.clear();

    for (const auto &devVal : devices) {
        QJsonObject dev = devVal.toObject();
        QString name = dev.value("name").toString();
        QString id = dev.value("id").toString();
        if (!name.isEmpty() && !id.isEmpty()) {
            m_deviceComboBox->addItem(name);
            m_deviceIds.insert(name, id);
        }
    }
}

void SpotifyTab::loadCoverArt(const QString &url) {
    if (url.isEmpty()) {
        m_coverArtLabel->setText("♫");
        m_coverArtLabel->setPixmap(QPixmap());
        return;
    }

    QNetworkRequest request(url);
    QNetworkReply *reply = m_coverArtLoader->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QImage img;
            if (img.loadFromData(data)) {
                m_coverArtLabel->setText("");
                m_coverArtLabel->setPixmap(QPixmap::fromImage(img).scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        reply->deleteLater();
    });
}
