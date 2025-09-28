#include "StatusParser.h"

#include <QStringList>
#include <QRegularExpression>

namespace {
ProcessState parseState(const QString &value)
{
    return processStateFromString(value.trimmed());
}

double parseDouble(const QString &value)
{
    bool ok = false;
    double result = value.trimmed().toDouble(&ok);
    return ok ? result : qQNaN();
}

} // namespace

StatusParser::StatusParser(QObject *parent)
    : QObject(parent)
{
}

StatusFrame StatusParser::parseStatusLine(const QString &line) const
{
    StatusFrame frame;
    frame.timestamp = QDateTime::currentDateTime();
    frame.rawFields.clear();

    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return frame;
    }

    QString payload = trimmed;
    if (payload.startsWith("STATUS")) {
        payload = payload.mid(QStringLiteral("STATUS").length()).trimmed();
    } else if (payload.startsWith("DONE")) {
        payload = payload.mid(QStringLiteral("DONE").length()).trimmed();
        frame.state = ProcessState::Done;
    } else if (payload.startsWith("FAULT")) {
        payload = payload.mid(QStringLiteral("FAULT").length()).trimmed();
        frame.state = ProcessState::Fault;
    }

    const QStringList tokens = payload.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const int colonIndex = token.indexOf(QLatin1Char(':'));
        if (colonIndex <= 0) {
            continue;
        }
        const QString key = token.left(colonIndex);
        const QString value = token.mid(colonIndex + 1);
        frame.rawFields.insert(key, value);

        if (key == QLatin1String("PH")) {
            frame.ph = parseDouble(value);
        } else if (key == QLatin1String("TARGET_PH")) {
            frame.targetPh = parseDouble(value);
        } else if (key == QLatin1String("DELTA_PH")) {
            frame.deltaPh = parseDouble(value);
        } else if (key == QLatin1String("EC")) {
            frame.ec = parseDouble(value);
        } else if (key == QLatin1String("TDS")) {
            frame.tds = parseDouble(value);
        } else if (key == QLatin1String("T")) {
            frame.temperature = parseDouble(value);
        } else if (key == QLatin1String("VCC")) {
            frame.vcc = parseDouble(value);
        } else if (key == QLatin1String("STATE")) {
            frame.state = parseState(value);
        } else if (key == QLatin1String("DOSE_A")) {
            frame.doses.fertAMl = parseDouble(value);
        } else if (key == QLatin1String("DOSE_B")) {
            frame.doses.fertBMl = parseDouble(value);
        } else if (key == QLatin1String("DOSE_UP")) {
            frame.doses.phUpMl = parseDouble(value);
        } else if (key == QLatin1String("DOSE_DOWN")) {
            frame.doses.phDownMl = parseDouble(value);
        } else if (key == QLatin1String("code")) {
            frame.faultCode = value.toInt();
        } else if (key == QLatin1String("msg")) {
            QString message = value;
            if (message.startsWith('"') && message.endsWith('"') && message.length() >= 2) {
                message = message.mid(1, message.length() - 2);
            }
            frame.faultMessage = message;
        }
    }

    return frame;
}

bool StatusParser::isStatusLine(const QString &line) const
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith("STATUS");
}

bool StatusParser::isDoneLine(const QString &line) const
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith("DONE");
}

bool StatusParser::isFaultLine(const QString &line) const
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith("FAULT");
}
