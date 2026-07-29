#include "BurstWorker.h"
#include <QDir>

BurstWorker::BurstWorker(QObject* parent)
    : QObject(parent)
{
}

void BurstWorker::onFrame(QImage frame) {
    QMutexLocker lock(&m_mutex);
    if (!m_active) return;
    m_queue.append(std::move(frame));
    m_queuedCount++;
    emit burstProgress(qMin(m_queuedCount, m_targetCount), m_targetCount);
    if (m_queuedCount >= m_targetCount) {
        m_active = false;
        lock.unlock();
        flush();
    }
}

void BurstWorker::startBurst(const QString& saveDir, int count, const QString& prefix) {
    QMutexLocker lock(&m_mutex);
    m_saveDir = saveDir;
    m_prefix = prefix;
    m_targetCount = count;
    m_queuedCount = 0;
    m_queue.clear();
    m_active = true;
}

void BurstWorker::abort() {
    QMutexLocker lock(&m_mutex);
    m_active = false;
    int n = m_queue.size();
    lock.unlock();
    if (n > 0)
        flush();
    else
        emit burstFinished(0);
}

void BurstWorker::flush() {
    QMutexLocker lock(&m_mutex);
    QList<QImage> batch = std::move(m_queue);
    int total = batch.size();
    lock.unlock();

    QDir dir(m_saveDir);
    if (!dir.exists())
        dir.mkpath(".");

    int saved = 0;
    for (int i = 0; i < total; i++) {
        QString path = dir.filePath(QString("%1_%2.tiff")
            .arg(m_prefix).arg(i + 1, 5, 10, QChar('0')));
        if (batch[i].save(path, "TIFF"))
            saved++;
    }
    emit burstFinished(saved);
}
