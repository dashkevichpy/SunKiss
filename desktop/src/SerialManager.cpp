#include "SerialManager.h"

#include <QDebug>

static constexpr int kStatusIntervalMs = 5000;
static constexpr int kQueueIntervalMs = 200;

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialManager::handleReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        emit errorOccurred(m_port.errorString());
    });

    m_queueTimer.setInterval(kQueueIntervalMs);
    connect(&m_queueTimer, &QTimer::timeout, this, &SerialManager::flushQueue);

    m_statusTimer.setInterval(kStatusIntervalMs);
    connect(&m_statusTimer, &QTimer::timeout, this, &SerialManager::requestStatus);
}

QString SerialManager::portName() const
{
    return m_portName;
}

void SerialManager::setPortName(const QString &name)
{
    m_portName = name;
}

bool SerialManager::isOpen() const
{
    return m_port.isOpen();
}

void SerialManager::open()
{
    if (m_portName.isEmpty()) {
        emit errorOccurred(tr("Serial port is not selected"));
        return;
    }
    if (m_port.isOpen()) {
        return;
    }

    m_port.setPortName(m_portName);
    m_port.setBaudRate(QSerialPort::Baud115200);
    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_port.errorString());
        return;
    }

    m_buffer.clear();
    m_queue.clear();
    m_queueTimer.start();
    m_statusTimer.start();
    emit connected();
}

void SerialManager::close()
{
    if (!m_port.isOpen()) {
        return;
    }
    m_statusTimer.stop();
    m_queueTimer.stop();
    m_port.close();
    emit disconnected();
}

void SerialManager::sendCommand(const QString &command, std::function<void(const QString &)> callback)
{
    QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!trimmed.endsWith('\n')) {
        trimmed.append('\n');
    }
    m_queue.enqueue(trimmed, std::move(callback));
    if (!m_queueTimer.isActive()) {
        m_queueTimer.start();
    }
}

void SerialManager::requestStatus()
{
    sendCommand(QStringLiteral("READ_NOW"));
}

void SerialManager::handleReadyRead()
{
    m_buffer.append(QString::fromUtf8(m_port.readAll()));

    int newlineIndex = -1;
    while ((newlineIndex = m_buffer.indexOf('\n')) >= 0) {
        const QString line = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);
        handleLine(line);
    }
}

void SerialManager::flushQueue()
{
    if (!m_port.isOpen()) {
        return;
    }
    if (!m_queue.hasNext()) {
        return;
    }

    PendingCommand pending = m_queue.takeNext();
    const QByteArray data = pending.command.toUtf8();
    m_port.write(data);
    emit logLine(QStringLiteral("> %1").arg(QString::fromUtf8(data.trimmed())));
    if (pending.onResponse) {
        pending.onResponse(QString());
    }
}

void SerialManager::handleLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    emit logLine(trimmed);
    emit rawFrameReceived(trimmed);

    if (m_parser.isStatusLine(trimmed) || m_parser.isDoneLine(trimmed) || m_parser.isFaultLine(trimmed)) {
        const StatusFrame frame = m_parser.parseStatusLine(trimmed);
        emit statusReceived(frame);
        if (m_parser.isFaultLine(trimmed)) {
            emit faultReceived(frame);
        } else if (m_parser.isDoneLine(trimmed)) {
            emit doneReceived(frame);
        }
    }
}
