#pragma once

#include "CommandQueue.h"
#include "StatusFrame.h"
#include "StatusParser.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

class SerialManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);

    QString portName() const;
    void setPortName(const QString &name);

    bool isOpen() const;

public slots:
    void open();
    void close();
    void sendCommand(const QString &command, std::function<void(const QString &)> callback = {});
    void requestStatus();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logLine(const QString &line);
    void statusReceived(const StatusFrame &frame);
    void rawFrameReceived(const QString &line);
    void faultReceived(const StatusFrame &frame);
    void doneReceived(const StatusFrame &frame);

private slots:
    void handleReadyRead();
    void flushQueue();

private:
    void handleLine(const QString &line);

    QSerialPort m_port;
    QString m_portName;
    QString m_buffer;
    CommandQueue m_queue;
    QTimer m_queueTimer;
    QTimer m_statusTimer;
    StatusParser m_parser;
};
