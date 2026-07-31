#include "BurstWorker.h"
#include <QDir>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

BurstWorker::BurstWorker(QObject* parent)
    : QObject(parent)
{
}

void BurstWorker::onRawFrame(int cv_type, int width, int height, QByteArray data) {
    QMutexLocker lock(&m_mutex);
    if (!m_active) return;
    m_queue.append({cv_type, width, height, std::move(data)});
    m_queuedCount++;
    emit burstProgress(qMin(m_queuedCount, m_targetCount), m_targetCount);
    if (m_queuedCount >= m_targetCount) {
        m_active = false;
        lock.unlock();
        flush();
    }
}

void BurstWorker::startBurst(const QString& saveDir, int count, const QString& prefix, bool denoise) {
    QMutexLocker lock(&m_mutex);
    m_saveDir = saveDir;
    m_prefix = prefix;
    m_targetCount = count;
    m_queuedCount = 0;
    m_queue.clear();
    m_active = true;
    m_denoise = denoise;
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

static bool saveFrame(const BurstFrame& f, const QString& path, bool denoise) {
    cv::Mat img(f.height, f.width, f.cv_type, const_cast<char*>(f.data.constData()));
    if (denoise && f.cv_type == CV_16UC1) {
        cv::medianBlur(img, img, 3);
    } else if (denoise && (f.cv_type == CV_8UC1 || f.cv_type == CV_8UC3)) {
        cv::medianBlur(img, img, 3);
    }
    std::vector<int> params = { cv::IMWRITE_TIFF_COMPRESSION, 1 };
    return cv::imwrite(path.toStdString(), img, params);
}

void BurstWorker::flush() {
    QMutexLocker lock(&m_mutex);
    QList<BurstFrame> batch = std::move(m_queue);
    int total = batch.size();
    lock.unlock();

    QDir dir(m_saveDir);
    if (!dir.exists())
        dir.mkpath(".");

    int saved = 0;
    for (int i = 0; i < total; i++) {
        QString path = dir.filePath(QString("%1_%2.tiff")
            .arg(m_prefix).arg(i + 1, 5, 10, QChar('0')));
        if (saveFrame(batch[i], path, m_denoise))
            saved++;
    }
    emit burstFinished(saved);
}
