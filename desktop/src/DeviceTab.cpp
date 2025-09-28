#include "DeviceTab.h"

#include "Utils.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QSerialPortInfo>

DeviceTab::DeviceTab(Settings *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_serial(new SerialManager(this))
    , m_processView(new ProcessView(this))
    , m_serviceView(new ServiceView(this))
    , m_logger(new Logger(this))
    , m_logView(new QTableView(this))
    , m_portCombo(new QComboBox(this))
    , m_connectButton(new QPushButton(tr("Подключить"), this))
    , m_disconnectButton(new QPushButton(tr("Отключить"), this))
{
    auto *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(new QLabel(tr("Порт:")));
    headerLayout->addWidget(m_portCombo);
    headerLayout->addWidget(m_connectButton);
    headerLayout->addWidget(m_disconnectButton);
    headerLayout->addStretch();

    auto *views = new QTabWidget(this);
    views->addTab(m_processView, tr("Процесс"));
    views->addTab(m_serviceView, tr("Сервис"));

    m_logView->setModel(m_logger);
    m_logView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(headerLayout);
    layout->addWidget(views, 1);
    layout->addWidget(new QLabel(tr("Журнал")));
    layout->addWidget(m_logView, 1);

    connect(m_connectButton, &QPushButton::clicked, this, &DeviceTab::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DeviceTab::onDisconnectClicked);

    connect(m_processView, &ProcessView::startRequested, this, &DeviceTab::onStartRequested);
    connect(m_processView, &ProcessView::abortRequested, this, &DeviceTab::onAbortRequested);
    connect(m_processView, &ProcessView::readNowRequested, this, &DeviceTab::onReadRequested);
    connect(m_processView, &ProcessView::mixOnlyRequested, this, &DeviceTab::onMixOnlyRequested);

    connect(m_serviceView, &ServiceView::sendCommandRequested, m_serial, &SerialManager::sendCommand);
    connect(m_serviceView, &ServiceView::exportLogRequested, this, [this](const QString &path) {
        if (!m_logger->exportCsv(path)) {
            QMessageBox::warning(this, tr("Экспорт"), tr("Не удалось записать файл"));
        }
    });

    connect(m_serial, &SerialManager::statusReceived, this, &DeviceTab::onStatusReceived);
    connect(m_serial, &SerialManager::faultReceived, this, &DeviceTab::onFaultReceived);
    connect(m_serial, &SerialManager::doneReceived, this, &DeviceTab::onDoneReceived);
    connect(m_serial, &SerialManager::errorOccurred, this, &DeviceTab::onError);
    connect(m_serial, &SerialManager::logLine, [this](const QString &line) {
        m_logger->addEntry(QStringLiteral("serial"), line);
    });

    refreshPortList();
    updatePortStatus();
}

void DeviceTab::bindDevice(DeviceBinding binding)
{
    m_binding = binding;
    m_processView->setProfile(binding.lastProfile);
    if (!binding.portName.isEmpty()) {
        m_serial->setPortName(binding.portName);
    }
    refreshPortList();
}

DeviceBinding DeviceTab::binding() const
{
    DeviceBinding updated = m_binding;
    updated.portName = m_serial->portName();
    updated.lastProfile = m_processView->profile();
    return updated;
}

void DeviceTab::refreshPortList()
{
    const QString selected = m_serial->portName();
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        m_portCombo->addItem(info.portName());
    }
    const int index = m_portCombo->findText(selected);
    if (index >= 0) {
        m_portCombo->setCurrentIndex(index);
    }
}

void DeviceTab::onConnectClicked()
{
    const QString port = m_portCombo->currentText();
    m_serial->setPortName(port);
    m_serial->open();
    m_binding.portName = port;
    emit requestSave();
    updatePortStatus();
}

void DeviceTab::onDisconnectClicked()
{
    m_serial->close();
    updatePortStatus();
}

void DeviceTab::onStartRequested(const RecipeProfile &profile)
{
    m_logger->addEntry(QStringLiteral("command"), tr("START"));
    m_binding.lastProfile = profile;
    emit requestSave();
    m_serial->sendCommand(QStringLiteral("SET_BATCH_L %1").arg(profile.batchLiters, 0, 'f', 1));
    m_serial->sendCommand(QStringLiteral("SET_TARGET_PH %1").arg(profile.targetPh, 0, 'f', 2));
    m_serial->sendCommand(QStringLiteral("SET_DOSE_A_ML_PER_L %1").arg(profile.fertAMlPerL, 0, 'f', 2));
    m_serial->sendCommand(QStringLiteral("SET_DOSE_B_ML_PER_L %1").arg(profile.fertBMlPerL, 0, 'f', 2));
    m_serial->sendCommand(QStringLiteral("SET_T %1").arg(profile.temperature, 0, 'f', 1));
    m_serial->sendCommand(QStringLiteral("START"));
}

void DeviceTab::onAbortRequested(const QString &reason)
{
    m_logger->addEntry(QStringLiteral("command"), tr("ABORT %1").arg(reason));
    m_serial->sendCommand(QStringLiteral("ABORT %1").arg(reason));
}

void DeviceTab::onReadRequested()
{
    m_serial->requestStatus();
}

void DeviceTab::onMixOnlyRequested(int durationMs)
{
    m_serial->sendCommand(QStringLiteral("MIX_ONLY %1").arg(durationMs));
}

void DeviceTab::onStatusReceived(const StatusFrame &frame)
{
    m_processView->applyStatus(frame);
    m_serviceView->applyStatus(frame);
}

void DeviceTab::onFaultReceived(const StatusFrame &frame)
{
    m_processView->handleFault(frame);
    announceCompletion(tr("Ошибка: %1").arg(frame.faultMessage));
    QMessageBox::critical(this, tr("FAULT"),
                          tr("Код %1\n%2\nДозы: up %3 ml, down %4 ml, A %5 ml, B %6 ml")
                              .arg(frame.faultCode)
                              .arg(frame.faultMessage)
                              .arg(Utils::formatDouble(frame.doses.phUpMl))
                              .arg(Utils::formatDouble(frame.doses.phDownMl))
                              .arg(Utils::formatDouble(frame.doses.fertAMl))
                              .arg(Utils::formatDouble(frame.doses.fertBMl)));
    emit requestSave();
}

void DeviceTab::onDoneReceived(const StatusFrame &frame)
{
    announceCompletion(tr("Компот готов"));
    m_processView->applyStatus(frame);
    emit requestSave();
}

void DeviceTab::onError(const QString &message)
{
    QMessageBox::warning(this, tr("Serial"), message);
}

void DeviceTab::updatePortStatus()
{
    const bool connected = m_serial->isOpen();
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
}

void DeviceTab::announceCompletion(const QString &message)
{
#ifdef Q_OS_MAC
    QProcess::execute(QStringLiteral("say"), {message});
#elif defined(Q_OS_LINUX)
    QProcess::execute(QStringLiteral("espeak"), {message});
#else
    Q_UNUSED(message)
#endif
}
