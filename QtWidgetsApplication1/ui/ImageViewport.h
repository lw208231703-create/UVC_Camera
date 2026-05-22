#pragma once

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QElapsedTimer>

class ImageViewport : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewport(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void clearImage();
    bool hasImage() const { return !m_pixmap.isNull(); }

    void setOverlayText(const QString& text);
    void setShowCrosshair(bool show) { m_showCrosshair = show; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void updateDisplay();
    QPointF widgetToImage(const QPointF& pos) const;
    QPointF imageToWidget(const QPointF& pos) const;
    QRectF imageRect() const;

    QPixmap m_pixmap;
    QImage m_sourceImage;
    QString m_overlayText;

    double m_scale = 1.0;
    QPointF m_offset;
    QPointF m_panStart;
    bool m_panning = false;
    bool m_showCrosshair = false;
    bool m_fitToWindow = true;
};
