#include "Utils.h"

#include <QtMath>
#include <QObject>

QString Utils::formatDouble(double value, int precision, const QString &fallback)
{
    if (std::isnan(value)) {
        return fallback;
    }
    return QString::number(value, 'f', precision);
}

QColor Utils::stateColor(ProcessState state)
{
    switch (state) {
    case ProcessState::Idle:
    case ProcessState::Done:
        return QColor(46, 204, 113);
    case ProcessState::Mix:
    case ProcessState::PhCoarse:
    case ProcessState::PhFine:
    case ProcessState::FertA:
    case ProcessState::FertB:
        return QColor(52, 152, 219);
    case ProcessState::Fault:
        return QColor(231, 76, 60);
    }
    return QColor(149, 165, 166);
}

QString Utils::stateDisplayName(ProcessState state)
{
    switch (state) {
    case ProcessState::Idle:
        return QObject::tr("IDLE");
    case ProcessState::Mix:
        return QObject::tr("MIX");
    case ProcessState::PhCoarse:
        return QObject::tr("PH_COARSE");
    case ProcessState::PhFine:
        return QObject::tr("PH_FINE");
    case ProcessState::FertA:
        return QObject::tr("FERT_A");
    case ProcessState::FertB:
        return QObject::tr("FERT_B");
    case ProcessState::Done:
        return QObject::tr("DONE");
    case ProcessState::Fault:
        return QObject::tr("FAULT");
    }
    return QObject::tr("UNKNOWN");
}
