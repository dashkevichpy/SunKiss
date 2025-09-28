#pragma once

#include "StatusFrame.h"

#include <QAbstractTableModel>
#include <QDateTime>
#include <QVector>

struct LogEntry {
    QDateTime timestamp;
    QString category;
    QString message;
};

class Logger : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addEntry(const QString &category, const QString &message);
    void clear();

    bool exportCsv(const QString &filePath) const;

private:
    QVector<LogEntry> m_entries;
};
