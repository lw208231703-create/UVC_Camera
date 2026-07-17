#include "BitShiftSelector.h"
#include <QPainter>
#include <algorithm>

BitShiftSelector::BitShiftSelector(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(46);
    setCursor(Qt::PointingHandCursor);
    setValue(8);
}

void BitShiftSelector::setValue(int val) {
    val = qBound(m_min, val, m_max);
    if (m_value == val)
        return;
    m_value = val;
    update();
    emit valueChanged(m_value);
}

void BitShiftSelector::setRange(int min, int max) {
    m_min = min;
    m_max = max;
    setValue(qBound(m_min, m_value, m_max));
}

QSize BitShiftSelector::sizeHint() const {
    return QSize(320, 46);
}

QSize BitShiftSelector::minimumSizeHint() const {
    return QSize(160, 46);
}

void BitShiftSelector::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int margin = 2;
    const int labelH = 14;
    const int barY = labelH + 2;
    const int barH = height() - barY - margin;
    const int totalW = width() - margin * 2;
    const int cellCount = 16;
    const qreal cellW = static_cast<qreal>(totalW) / cellCount;

    // Whole bar background
    QRectF barRect(margin, barY, totalW, barH);
    p.fillRect(barRect, QColor("#3C3C3C"));

    // Highlight area: bits [m_value + 7 : m_value]
    int startBit = m_value + 7;
    int endBit = m_value;
    int startCell = 15 - startBit; // e.g. val=8 -> startBit=15 -> cell 0
    int endCell   = 15 - endBit;   // e.g. val=8 -> endBit=8   -> cell 7
    if (startCell > endCell)
        std::swap(startCell, endCell);

    qreal hlX = margin + startCell * cellW;
    qreal hlW = (endCell - startCell + 1) * cellW;
    QRectF hlRect(hlX, barY, hlW, barH);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#26C0A6"));
    p.drawRoundedRect(hlRect, 3, 3);

    // Vertical grid lines
    p.setPen(QColor("#2D2D30"));
    for (int i = 1; i < cellCount; ++i) {
        qreal x = margin + i * cellW;
        p.drawLine(QPointF(x, barY), QPointF(x, barY + barH));
    }

    // Outer border of the whole bar
    p.setPen(QColor("#2D2D30"));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(barRect, 3, 3);

    // MSB / LSB labels
    p.setPen(QColor("#858585"));
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    p.drawText(margin, 0, 40, labelH, Qt::AlignLeft | Qt::AlignVCenter, "MSB");
    p.drawText(width() - margin - 40, 0, 40, labelH, Qt::AlignRight | Qt::AlignVCenter, "LSB");

    // Cell numbers
    for (int i = 0; i < cellCount; ++i) {
        int bit = 15 - i;
        bool selected = (bit >= m_value && bit <= m_value + 7);
        QRectF rect(margin + i * cellW, barY, cellW, barH);
        p.setPen(selected ? Qt::white : QColor("#858585"));
        p.setFont(QFont("Consolas", 9, selected ? QFont::Bold : QFont::Normal));
        p.drawText(rect, Qt::AlignCenter, QString::number(bit));
    }
}

void BitShiftSelector::handleMouse(int x) {
    const int margin = 2;
    const int totalW = width() - margin * 2;
    const int cellCount = 16;

    int cell = (x - margin) * cellCount / totalW;
    cell = qBound(0, cell, cellCount - 1);
    int bit = 15 - cell;

    // Try to center the clicked bit inside the 8-bit window
    int newVal = bit - 3;
    newVal = qBound(m_min, newVal, m_max);

    // Make sure the clicked bit really falls inside the window
    if (bit < newVal)
        newVal = qBound(m_min, bit, m_max);
    if (bit > newVal + 7)
        newVal = qBound(m_min, bit - 7, m_max);

    setValue(newVal);
}

void BitShiftSelector::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartX = event->pos().x();
        m_dragStartVal = m_value;
        handleMouse(event->pos().x());
    }
}

void BitShiftSelector::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging)
        return;
    handleMouse(event->pos().x());
}

void BitShiftSelector::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}
