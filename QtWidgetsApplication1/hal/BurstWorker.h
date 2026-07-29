#pragma once

#include <QObject>
#include <QMutex>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QAtomicInt>

struct BurstFrame {
    int cv_type;
    int width;
    int height;
    QByteArray data;
};

class BurstWorker : public QObject {
    Q_OBJECT
public:
    explicit BurstWorker(QObject* parent = nullptr);

public slots:
    void onRawFrame(int cv_type, int width, int height, QByteArray data);
    void startBurst(const QString& saveDir, int count, const QString& prefix, bool denoise = false);
    void abort();

signals:
    void burstProgress(int current, int total);
    void burstFinished(int saved);

private:
    void flush();

    QMutex m_mutex;
    QList<BurstFrame> m_queue;
    QString m_saveDir;
    QString m_prefix;
    int m_targetCount = 0;
    int m_queuedCount = 0;
    bool m_active = false;
    bool m_denoise = false;
};
