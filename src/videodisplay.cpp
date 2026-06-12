#include "videodisplay.h"
#include <QPainter>
#include <QPaintEvent>
#include <QSvgRenderer>

VideoDisplay::VideoDisplay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    auto palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    setMouseTracking(true);
}


void VideoDisplay::setBackground(const QPixmap &pixmap)
{
    m_useDefaultBg = false;
    m_currentBg = pixmap;
    update();
}

void VideoDisplay::useDefaultBackground()
{
    m_useDefaultBg = true;
    update();
}

void VideoDisplay::setHasActiveVideo(const bool &value)
{
    if (m_hasActiveVideo != value)
    {
        m_hasActiveVideo = value;
        if (!m_hasActiveVideo)
        {
            m_currentFrame = QImage();
        }
        update();
    }
}

void VideoDisplay::updateFrame(const QImage &image)
{
    m_currentFrame = image;
    m_hasActiveVideo = true;
    update();
}

void VideoDisplay::clearFrame()
{
    m_currentFrame = QImage();
    m_hasActiveVideo = false;
    update();
}

void VideoDisplay::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (m_hasActiveVideo && !m_currentFrame.isNull())
    {
        painter.fillRect(event->rect(), Qt::black);
        QImage scaled = m_currentFrame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawImage(x, y, scaled);
    }
    else
    {
        // stopped - draw background image
        painter.fillRect(event->rect(), Qt::black);
        if (m_useDefaultBg)
        {
            QSvgRenderer renderer(QString(":icons/Icons/okjlogo.svg"));
            renderer.setAspectRatioMode(Qt::KeepAspectRatio);
            renderer.render(&painter);
        }
        else
        {
            painter.drawPixmap(rect(), m_currentBg, m_currentBg.rect());
        }
    }
}
