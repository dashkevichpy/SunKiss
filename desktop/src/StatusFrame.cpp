#include "StatusFrame.h"

#include <QString>

QString processStateToString(ProcessState state)
{
    switch (state) {
    case ProcessState::Idle:
        return QStringLiteral("IDLE");
    case ProcessState::Mix:
        return QStringLiteral("MIX");
    case ProcessState::PhCoarse:
        return QStringLiteral("PH_COARSE");
    case ProcessState::PhFine:
        return QStringLiteral("PH_FINE");
    case ProcessState::FertA:
        return QStringLiteral("FERT_A");
    case ProcessState::FertB:
        return QStringLiteral("FERT_B");
    case ProcessState::Done:
        return QStringLiteral("DONE");
    case ProcessState::Fault:
        return QStringLiteral("FAULT");
    }
    return QStringLiteral("UNKNOWN");
}

ProcessState processStateFromString(const QString &value)
{
    const QString upper = value.trimmed().toUpper();
    if (upper == QLatin1String("IDLE")) {
        return ProcessState::Idle;
    }
    if (upper == QLatin1String("MIX")) {
        return ProcessState::Mix;
    }
    if (upper == QLatin1String("PH_COARSE")) {
        return ProcessState::PhCoarse;
    }
    if (upper == QLatin1String("PH_FINE")) {
        return ProcessState::PhFine;
    }
    if (upper == QLatin1String("FERT_A")) {
        return ProcessState::FertA;
    }
    if (upper == QLatin1String("FERT_B")) {
        return ProcessState::FertB;
    }
    if (upper == QLatin1String("DONE")) {
        return ProcessState::Done;
    }
    if (upper == QLatin1String("FAULT")) {
        return ProcessState::Fault;
    }
    return ProcessState::Idle;
}
