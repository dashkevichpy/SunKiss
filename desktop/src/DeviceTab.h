#pragma once

#include "Logger.h"
#include "ProcessView.h"
#include "SerialManager.h"
#include "ServiceView.h"
#include "Settings.h"

#include <QWidget>

class QTabWidget;
class QTableView;
class QComboBox;
class QPushButton;

class DeviceTab : public QWidget
{
    Q_OBJECT
public:
    DeviceTab(Settings *settings, QWidget *parent = nullptr);

    void bindDevice(DeviceBinding binding);
    DeviceBinding binding() const;

signals:
    void requestSave();

public slots:
    void refreshPortList();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onStartRequested(const RecipeProfile &profile);
    void onAbortRequested(const QString &reason);
    void onReadRequested();
    void onMixOnlyRequested(int durationMs);
    void onStatusReceived(const StatusFrame &frame);
    void onFaultReceived(const StatusFrame &frame);
    void onDoneReceived(const StatusFrame &frame);
    void onError(const QString &message);

private:
    void updatePortStatus();
    void announceCompletion(const QString &message);

    Settings *m_settings;
    DeviceBinding m_binding;
    SerialManager *m_serial;
    ProcessView *m_processView;
    ServiceView *m_serviceView;
    Logger *m_logger;
    QTableView *m_logView;
    QComboBox *m_portCombo;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
};
