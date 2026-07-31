#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>

class StatsWorker : public QObject {
    Q_OBJECT
public:
    explicit StatsWorker(QObject* parent = nullptr);

    void setCounters(std::atomic<uint64_t>* camFrames,
                     std::atomic<uint64_t>* dispFrames,
                     std::atomic<uint64_t>* rxBytes);

public slots:
    void start();
    void stop();
    void reset();

signals:
    void statsReady(double camFps, double dispFps, double rxMbps);

private slots:
    void tick();

private:
    std::atomic<uint64_t>* m_camFrames = nullptr;
    std::atomic<uint64_t>* m_dispFrames = nullptr;
    std::atomic<uint64_t>* m_rxBytes = nullptr;

    QTimer*       m_timer = nullptr;
    QElapsedTimer m_elapsed;
    qint64   m_lastTime = 0;
    uint64_t m_lastCam = 0;
    uint64_t m_lastDisp = 0;
    uint64_t m_lastBytes = 0;
    int      m_emitCounter = 0;
};
