#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMutex>

class LogManager : public QObject {
    Q_OBJECT

public:
    enum Level { Debug, Info, Warning, Error };

    static LogManager& instance();

    void log(Level level, const QString& message);

    void debug(const QString& msg)   { log(Debug, msg); }
    void info(const QString& msg)    { log(Info, msg); }
    void warning(const QString& msg) { log(Warning, msg); }
    void error(const QString& msg)   { log(Error, msg); }

    struct Entry {
        Level level;
        QString timestamp;
        QString message;
    };

    const QList<Entry>& entries() const { return m_entries; }
    void clear();

signals:
    void newEntry(const LogManager::Entry& entry);

private:
    LogManager() = default;

    QMutex m_mutex;
    QList<Entry> m_entries;
    static constexpr int MaxEntries = 1000;
};

Q_DECLARE_METATYPE(LogManager::Entry)

// 取消注释下一行可完全关闭日志输出（编译时生效）
//#define UVC_DISABLE_LOG
#if defined(UVC_DISABLE_LOG)
#define LOG_DEBUG(msg)    ((void)0)
#define LOG_INFO(msg)     ((void)0)
#define LOG_WARNING(msg)  ((void)0)
#define LOG_ERROR(msg)    ((void)0)
#else
#define LOG_DEBUG(msg)    LogManager::instance().debug(msg)
#define LOG_INFO(msg)     LogManager::instance().info(msg)
#define LOG_WARNING(msg)  LogManager::instance().warning(msg)
#define LOG_ERROR(msg)    LogManager::instance().error(msg)
#endif
