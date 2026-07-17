#pragma once

#include <QWidget>
#include <QMouseEvent>

class BitShiftSelector : public QWidget {
    Q_OBJECT

public:
    explicit BitShiftSelector(QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int val);
    void setRange(int min, int max);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void handleMouse(int x);

    int m_value = 8;
    int m_min = 0;
    int m_max = 8;
    bool m_dragging = false;
    int m_dragStartX = 0;
    int m_dragStartVal = 0;
};
