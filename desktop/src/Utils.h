#pragma once

#include "StatusFrame.h"
#include <QString>
#include <QColor>

namespace Utils {
QString formatDouble(double value, int precision = 2, const QString &fallback = QStringLiteral("--"));
QColor stateColor(ProcessState state);
QString stateDisplayName(ProcessState state);
}
