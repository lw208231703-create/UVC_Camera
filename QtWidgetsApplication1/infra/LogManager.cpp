#include "LogManager.h"
#include <QDateTime>
#include <QMutexLocker>

LogManager& LogManager::instance() {
    static LogManager inst;
    return inst;
}

void LogManager::log(Level level, const QString& message) {
    Entry entry{
        level,
        QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
        message
    };

    {
        QMutexLocker lock(&m_mutex);
        m_entries.append(entry);
        while (m_entries.size() > MaxEntries) {
            m_entries.removeFirst();
        }
    }

    emit newEntry(entry);
}

void LogManager::clear() {
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
}
