#include "StatsWorker.h"

StatsWorker::StatsWorker(QObject* parent)
    : QObject(parent)
{
}

void StatsWorker::setCounters(std::atomic<uint64_t>* camFrames,
                              std::atomic<uint64_t>* dispFrames,
                              std::atomic<uint64_t>* rxBytes)
{
    m_camFrames = camFrames;
    m_dispFrames = dispFrames;
    m_rxBytes = rxBytes;
}

void StatsWorker::start() {
    m_elapsed.start();
    m_lastTime = m_elapsed.elapsed();
    m_lastCam = m_camFrames ? m_camFrames->load(std::memory_order_relaxed) : 0;
    m_lastDisp = m_dispFrames ? m_dispFrames->load(std::memory_order_relaxed) : 0;
    m_lastBytes = m_rxBytes ? m_rxBytes->load(std::memory_order_relaxed) : 0;

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &StatsWorker::tick);
    m_timer->start();
}

void StatsWorker::stop() {
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
}

void StatsWorker::reset() {
    m_lastTime = m_elapsed.elapsed();
    m_lastCam = m_camFrames ? m_camFrames->load(std::memory_order_relaxed) : 0;
    m_lastDisp = m_dispFrames ? m_dispFrames->load(std::memory_order_relaxed) : 0;
    m_lastBytes = m_rxBytes ? m_rxBytes->load(std::memory_order_relaxed) : 0;
    m_emitCounter = 0;
}

void StatsWorker::tick() {
    qint64 now = m_elapsed.elapsed();
    double dt = (now - m_lastTime) / 1000.0;
    m_lastTime = now;
    if (dt <= 0.0) return;

    uint64_t cam = m_camFrames ? m_camFrames->load(std::memory_order_relaxed) : 0;
    uint64_t disp = m_dispFrames ? m_dispFrames->load(std::memory_order_relaxed) : 0;
    uint64_t bytes = m_rxBytes ? m_rxBytes->load(std::memory_order_relaxed) : 0;

    // 1 秒窗口内收到的帧数/字节数直接作为帧率/速率
    double camFps = (double)(cam - m_lastCam) / dt;
    double dispFps = (double)(disp - m_lastDisp) / dt;
    double rxMbps = (double)(bytes - m_lastBytes) / dt / (1024.0 * 1024.0);
    m_lastCam = cam;
    m_lastDisp = disp;
    m_lastBytes = bytes;

    // 每秒计算，但每 5 秒才发信号刷新一次显示
    if (++m_emitCounter < 5)
        return;
    m_emitCounter = 0;
    emit statsReady(camFps, dispFps, rxMbps);
}
