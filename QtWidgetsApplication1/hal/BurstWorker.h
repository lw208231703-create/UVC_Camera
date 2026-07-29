#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QList>
#include <QImage>
#include <QString>
#include <QAtomicInt>

class BurstWorker : public QObject {
    Q_OBJECT
public:
    explicit BurstWorker(QObject* parent = nullptr);

public slots:
    void onFrame(QImage frame);
    void startBurst(const QString& saveDir, int count, const QString& prefix);
    void abort();

signals:
    void burstProgress(int current, int total);
    void burstFinished(int saved);

private:
    void flush();

    QMutex m_mutex;
    QList<QImage> m_queue;
    QString m_saveDir;
    QString m_prefix;
    int m_targetCount = 0;
    int m_queuedCount = 0;
    bool m_active = false;
};
