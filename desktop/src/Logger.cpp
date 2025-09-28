#include "Logger.h"

#include <QFile>
#include <QTextStream>

Logger::Logger(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int Logger::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

int Logger::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 3;
}

QVariant Logger::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const LogEntry &entry = m_entries.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return entry.timestamp.toString(Qt::ISODate);
        case 1:
            return entry.category;
        case 2:
            return entry.message;
        default:
            break;
        }
    }
    return QVariant();
}

QVariant Logger::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return tr("Time");
        case 1:
            return tr("Category");
        case 2:
            return tr("Message");
        default:
            break;
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void Logger::addEntry(const QString &category, const QString &message)
{
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append({QDateTime::currentDateTime(), category, message});
    endInsertRows();
}

void Logger::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

bool Logger::exportCsv(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "timestamp,category,message\n";
    for (const LogEntry &entry : m_entries) {
        QString category = entry.category;
        QString message = entry.message;
        category.replace('"', QLatin1String(""""));
        message.replace('"', QLatin1String(""""));
        stream << '"' << entry.timestamp.toString(Qt::ISODate) << "","" << category << "","" << message << '"' << "\n";
    }
    return true;
}
