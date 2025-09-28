#pragma once

#include "StatusFrame.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;

class ServiceView : public QWidget
{
    Q_OBJECT
public:
    explicit ServiceView(QWidget *parent = nullptr);

signals:
    void sendCommandRequested(const QString &command);
    void exportLogRequested(const QString &filePath);

public slots:
    void applyStatus(const StatusFrame &frame);

private slots:
    void onTestPumpClicked();
    void onSetPumpRateClicked();
    void onSetTemperatureClicked();
    void onSetCoefficientsClicked();
    void onSetIdClicked();
    void onResetClicked();
    void onExportLogClicked();

private:
    QComboBox *m_pumpChannel;
    QDoubleSpinBox *m_pumpVolume;
    QDoubleSpinBox *m_pumpRate;
    QDoubleSpinBox *m_temperature;
    QDoubleSpinBox *m_alpha;
    QDoubleSpinBox *m_kCell;
    QSpinBox *m_tdsFactor;
    QSpinBox *m_deviceId;
    QTextEdit *m_log;
};
