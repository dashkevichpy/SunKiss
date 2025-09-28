#pragma once

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QtGlobal>

enum class ProcessState {
    Idle,
    Mix,
    PhCoarse,
    PhFine,
    FertA,
    FertB,
    Done,
    Fault
};

struct DoseSummary {
    double phUpMl = 0.0;
    double phDownMl = 0.0;
    double fertAMl = 0.0;
    double fertBMl = 0.0;
};

struct StatusFrame {
    QDateTime timestamp;
    double ph = qQNaN();
    double targetPh = qQNaN();
    double deltaPh = qQNaN();
    double ec = qQNaN();
    double tds = qQNaN();
    double temperature = qQNaN();
    double vcc = qQNaN();
    ProcessState state = ProcessState::Idle;
    DoseSummary doses;
    int faultCode = 0;
    QString faultMessage;
    QMap<QString, QString> rawFields;
};

QString processStateToString(ProcessState state);
ProcessState processStateFromString(const QString &value);
