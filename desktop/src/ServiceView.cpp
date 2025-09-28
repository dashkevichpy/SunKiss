#include "ServiceView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFileDialog>

ServiceView::ServiceView(QWidget *parent)
    : QWidget(parent)
    , m_pumpChannel(new QComboBox(this))
    , m_pumpVolume(new QDoubleSpinBox(this))
    , m_pumpRate(new QDoubleSpinBox(this))
    , m_temperature(new QDoubleSpinBox(this))
    , m_alpha(new QDoubleSpinBox(this))
    , m_kCell(new QDoubleSpinBox(this))
    , m_tdsFactor(new QSpinBox(this))
    , m_deviceId(new QSpinBox(this))
    , m_log(new QTextEdit(this))
{
    m_pumpChannel->addItems({QStringLiteral("PH_DOWN"), QStringLiteral("PH_UP"), QStringLiteral("A"), QStringLiteral("B")});
    m_pumpVolume->setRange(0.1, 500.0);
    m_pumpVolume->setDecimals(1);
    m_pumpVolume->setSuffix(tr(" ml"));
    m_pumpRate->setRange(0.1, 50.0);
    m_pumpRate->setDecimals(2);
    m_pumpRate->setSuffix(tr(" ml/s"));

    m_temperature->setRange(0.0, 60.0);
    m_temperature->setDecimals(1);
    m_temperature->setSuffix(tr(" °C"));

    m_alpha->setRange(0.0, 0.1);
    m_alpha->setDecimals(4);
    m_alpha->setValue(0.02);

    m_kCell->setRange(0.1, 5.0);
    m_kCell->setDecimals(3);
    m_kCell->setValue(1.0);

    m_tdsFactor->setRange(400, 900);
    m_tdsFactor->setSingleStep(10);
    m_tdsFactor->setValue(500);

    m_deviceId->setRange(0, 255);

    m_log->setReadOnly(true);

    auto *pumpLayout = new QFormLayout;
    pumpLayout->addRow(tr("Канал"), m_pumpChannel);
    pumpLayout->addRow(tr("Объём (мл)"), m_pumpVolume);
    pumpLayout->addRow(tr("Расход (мл/с)"), m_pumpRate);

    auto *pumpBox = new QGroupBox(tr("Тест насосов"), this);
    pumpBox->setLayout(pumpLayout);

    auto *pumpButtonsLayout = new QHBoxLayout;
    auto *testPumpButton = new QPushButton(tr("Дозировать"), this);
    auto *setRateButton = new QPushButton(tr("Задать расход"), this);
    pumpButtonsLayout->addWidget(testPumpButton);
    pumpButtonsLayout->addWidget(setRateButton);
    pumpLayout->addRow(pumpButtonsLayout);

    connect(testPumpButton, &QPushButton::clicked, this, &ServiceView::onTestPumpClicked);
    connect(setRateButton, &QPushButton::clicked, this, &ServiceView::onSetPumpRateClicked);

    auto *calibrationLayout = new QFormLayout;
    calibrationLayout->addRow(tr("Температура раствора"), m_temperature);
    calibrationLayout->addRow(tr("α (EC)"), m_alpha);
    calibrationLayout->addRow(tr("K cell"), m_kCell);
    calibrationLayout->addRow(tr("TDS factor"), m_tdsFactor);
    calibrationLayout->addRow(tr("ID устройства"), m_deviceId);

    auto *calibrationBox = new QGroupBox(tr("Настройки"), this);
    calibrationBox->setLayout(calibrationLayout);

    auto *calibrationButtons = new QHBoxLayout;
    auto *setTempButton = new QPushButton(tr("Установить температуру"), this);
    auto *setCoeffButton = new QPushButton(tr("Установить коэффициенты"), this);
    auto *setIdButton = new QPushButton(tr("Сохранить ID"), this);
    auto *resetButton = new QPushButton(tr("Сброс EEPROM"), this);
    auto *exportButton = new QPushButton(tr("Экспорт журнала"), this);

    calibrationButtons->addWidget(setTempButton);
    calibrationButtons->addWidget(setCoeffButton);
    calibrationButtons->addWidget(setIdButton);
    calibrationButtons->addWidget(resetButton);
    calibrationButtons->addWidget(exportButton);
    calibrationButtons->addStretch();

    connect(setTempButton, &QPushButton::clicked, this, &ServiceView::onSetTemperatureClicked);
    connect(setCoeffButton, &QPushButton::clicked, this, &ServiceView::onSetCoefficientsClicked);
    connect(setIdButton, &QPushButton::clicked, this, &ServiceView::onSetIdClicked);
    connect(resetButton, &QPushButton::clicked, this, &ServiceView::onResetClicked);
    connect(exportButton, &QPushButton::clicked, this, &ServiceView::onExportLogClicked);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(pumpBox);
    layout->addWidget(calibrationBox);
    layout->addLayout(calibrationButtons);
    layout->addWidget(m_log, 1);
}

void ServiceView::applyStatus(const StatusFrame &frame)
{
    m_log->append(QStringLiteral("[%1] STATE %2")
                      .arg(frame.timestamp.toString(Qt::ISODate))
                      .arg(processStateToString(frame.state)));
}

void ServiceView::onTestPumpClicked()
{
    const QString channel = m_pumpChannel->currentText();
    const double volume = m_pumpVolume->value();
    emit sendCommandRequested(QStringLiteral("TEST_PUMP %1 %2").arg(channel).arg(volume, 0, 'f', 1));
}

void ServiceView::onSetPumpRateClicked()
{
    const QString channel = m_pumpChannel->currentText();
    const double rate = m_pumpRate->value();
    emit sendCommandRequested(QStringLiteral("SET_PUMP_RATE %1 %2").arg(channel).arg(rate, 0, 'f', 2));
}

void ServiceView::onSetTemperatureClicked()
{
    const double temp = m_temperature->value();
    emit sendCommandRequested(QStringLiteral("SET_T %1").arg(temp, 0, 'f', 1));
}

void ServiceView::onSetCoefficientsClicked()
{
    emit sendCommandRequested(QStringLiteral("SET_EC_ALPHA %1").arg(m_alpha->value(), 0, 'f', 4));
    emit sendCommandRequested(QStringLiteral("SET_K %1").arg(m_kCell->value(), 0, 'f', 3));
    emit sendCommandRequested(QStringLiteral("SET_TDSFACTOR %1").arg(m_tdsFactor->value()));
}

void ServiceView::onSetIdClicked()
{
    emit sendCommandRequested(QStringLiteral("SET_ID %1").arg(m_deviceId->value()));
}

void ServiceView::onResetClicked()
{
    emit sendCommandRequested(QStringLiteral("RESET_CONFIG"));
}

void ServiceView::onExportLogClicked()
{
    const QString filePath = QFileDialog::getSaveFileName(this, tr("Экспорт журнала"), QString(), tr("CSV (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }
    emit exportLogRequested(filePath);
}
