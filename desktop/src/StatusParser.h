#pragma once

#include "StatusFrame.h"

#include <QObject>
#include <QStringList>

class StatusParser : public QObject
{
    Q_OBJECT
public:
    explicit StatusParser(QObject *parent = nullptr);

    StatusFrame parseStatusLine(const QString &line) const;
    bool isStatusLine(const QString &line) const;
    bool isDoneLine(const QString &line) const;
    bool isFaultLine(const QString &line) const;
};
