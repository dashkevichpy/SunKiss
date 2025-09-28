#pragma once

#include <QObject>
#include <QDateTime>
#include <QQueue>
#include <QString>
#include <functional>
#include <utility>

struct PendingCommand {
    QString command;
    std::function<void(const QString &)> onResponse;
    QDateTime enqueuedAt;
};

class CommandQueue : public QObject
{
    Q_OBJECT
public:
    explicit CommandQueue(QObject *parent = nullptr);

    void enqueue(const QString &cmd, std::function<void(const QString &)> onResponse = {});
    bool hasNext() const;
    PendingCommand takeNext();
    void clear();

private:
    QQueue<PendingCommand> m_queue;
};

inline CommandQueue::CommandQueue(QObject *parent)
    : QObject(parent)
{
}

inline void CommandQueue::enqueue(const QString &cmd, std::function<void(const QString &)> onResponse)
{
    PendingCommand pending{cmd, std::move(onResponse), QDateTime::currentDateTime()};
    m_queue.enqueue(std::move(pending));
}

inline bool CommandQueue::hasNext() const
{
    return !m_queue.isEmpty();
}

inline PendingCommand CommandQueue::takeNext()
{
    return m_queue.dequeue();
}

inline void CommandQueue::clear()
{
    m_queue.clear();
}
