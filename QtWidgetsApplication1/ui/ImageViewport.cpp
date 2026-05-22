#include "ImageViewport.h"
#include "infra/UiStrings.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFontMetrics>

ImageViewport::ImageViewport(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageViewport::setImage(const QImage& image) {
    m_sourceImage = image;
    if (m_fitToWindow)
        updateDisplay();
    m_pixmap = QPixmap::fromImage(m_sourceImage);
    update();
}

void ImageViewport::clearImage() {
    m_pixmap = QPixmap();
    m_sourceImage = QImage();
    update();
}

void ImageViewport::setOverlayText(const QString& text) {
    m_overlayText = text;
    update();
}

void ImageViewport::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Dark background
    p.fillRect(rect(), QColor("#1E1E1E"));

    if (!m_pixmap.isNull()) {
        QRectF ir = imageRect();
        p.drawPixmap(ir, m_pixmap, m_pixmap.rect());
    } else {
        // Offline placeholder
        p.setPen(QColor("#555555"));
        QFont f = font();
        f.setPointSize(18);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, TR("Device Offline"));
    }

    // HUD overlay (top-left)
    if (!m_overlayText.isEmpty()) {
        QFont f("Consolas", 11);
        p.setFont(f);
        p.setPen(QColor("#26C0A6"));
        QFontMetrics fm(f);
        QRect hudRect(8, 8, fm.horizontalAdvance(m_overlayText) + 20, fm.height() + 8);
        p.fillRect(hudRect, QColor(0, 0, 0, 160));
        p.drawText(hudRect, Qt::AlignCenter, m_overlayText);
    }
}

void ImageViewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_fitToWindow && !m_sourceImage.isNull())
        updateDisplay();
}

void ImageViewport::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_scale = qBound(0.05, m_scale * factor, 20.0);
    m_fitToWindow = false;
    update();
}

void ImageViewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_pixmap.isNull()) {
        m_panStart = event->position();
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageViewport::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        QPointF delta = event->position() - m_panStart;
        m_offset += delta;
        m_panStart = event->position();
        m_fitToWindow = false;
        update();
    }
}

void ImageViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageViewport::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        m_scale = 1.0;
        m_offset = QPointF(0, 0);
        m_fitToWindow = true;
        updateDisplay();
    }
}

void ImageViewport::updateDisplay() {
    if (m_sourceImage.isNull() || !m_fitToWindow) return;
    QSize sz = m_sourceImage.size();
    sz.scale(size(), Qt::KeepAspectRatio);
    m_pixmap = QPixmap::fromImage(m_sourceImage.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QRectF ImageViewport::imageRect() const {
    if (m_pixmap.isNull()) return QRectF();
    QSizeF sz = m_pixmap.size() * m_scale;
    QPointF topLeft = QPointF((width() - sz.width()) / 2.0, (height() - sz.height()) / 2.0) + m_offset;
    return QRectF(topLeft, sz);
}

QPointF ImageViewport::widgetToImage(const QPointF& pos) const {
    QRectF ir = imageRect();
    return (pos - ir.topLeft()) / m_scale;
}

QPointF ImageViewport::imageToWidget(const QPointF& pos) const {
    return pos * m_scale + imageRect().topLeft();
}
