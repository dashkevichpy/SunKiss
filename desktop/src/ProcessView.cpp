#include "ProcessView.h"

#include "Utils.h"

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

ProcessView::ProcessView(QWidget *parent)
    : QWidget(parent)
    , m_batchLiters(new QDoubleSpinBox(this))
    , m_targetPh(new QDoubleSpinBox(this))
    , m_fertA(new QDoubleSpinBox(this))
    , m_fertB(new QDoubleSpinBox(this))
    , m_temperature(new QDoubleSpinBox(this))
    , m_currentPh(new QLabel(tr("--"), this))
    , m_targetPhLabel(new QLabel(tr("--"), this))
    , m_deltaPh(new QLabel(tr("--"), this))
    , m_ec(new QLabel(tr("--"), this))
    , m_tds(new QLabel(tr("--"), this))
    , m_temp(new QLabel(tr("--"), this))
    , m_state(new QLabel(tr("IDLE"), this))
    , m_doseUp(new QLabel(tr("0 ml"), this))
    , m_doseDown(new QLabel(tr("0 ml"), this))
    , m_doseA(new QLabel(tr("0 ml"), this))
    , m_doseB(new QLabel(tr("0 ml"), this))
    , m_progress(new QProgressBar(this))
    , m_log(new QTextEdit(this))
{
    m_batchLiters->setRange(1.0, 250.0);
    m_batchLiters->setSuffix(tr(" L"));
    m_batchLiters->setValue(20.0);

    m_targetPh->setRange(4.5, 7.5);
    m_targetPh->setDecimals(2);
    m_targetPh->setValue(5.8);

    m_fertA->setRange(0.0, 20.0);
    m_fertA->setDecimals(2);

    m_fertB->setRange(0.0, 20.0);
    m_fertB->setDecimals(2);

    m_temperature->setRange(0.0, 60.0);
    m_temperature->setDecimals(1);
    m_temperature->setValue(24.0);
    m_temperature->setSuffix(tr(" °C"));

    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    m_log->setReadOnly(true);

    auto *inputsLayout = new QFormLayout;
    inputsLayout->addRow(tr("Объём (л)"), m_batchLiters);
    inputsLayout->addRow(tr("Целевой pH"), m_targetPh);
    inputsLayout->addRow(tr("Удобрение A (мл/л)"), m_fertA);
    inputsLayout->addRow(tr("Удобрение B (мл/л)"), m_fertB);
    inputsLayout->addRow(tr("Температура (°C)"), m_temperature);

    auto *inputsBox = new QGroupBox(tr("Параметры партии"), this);
    inputsBox->setLayout(inputsLayout);

    auto makeInfoRow = [](const QString &title, QLabel *label) {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel(title));
        layout->addStretch();
        layout->addWidget(label);
        return row;
    };

    auto *infoLayout = new QVBoxLayout;
    infoLayout->addWidget(makeInfoRow(tr("📊 pH"), m_currentPh));
    infoLayout->addWidget(makeInfoRow(tr("🎯 Цель pH"), m_targetPhLabel));
    infoLayout->addWidget(makeInfoRow(tr("⏱️ ΔpH"), m_deltaPh));
    infoLayout->addWidget(makeInfoRow(tr("⚡ EC (mS/cm)"), m_ec));
    infoLayout->addWidget(makeInfoRow(tr("💧 TDS (ppm)"), m_tds));
    infoLayout->addWidget(makeInfoRow(tr("🌡️ Температура"), m_temp));
    infoLayout->addWidget(makeInfoRow(tr("🧪 Состояние"), m_state));
    infoLayout->addWidget(makeInfoRow(tr("⬆️ pH Up"), m_doseUp));
    infoLayout->addWidget(makeInfoRow(tr("⬇️ pH Down"), m_doseDown));
    infoLayout->addWidget(makeInfoRow(tr("🧴 A"), m_doseA));
    infoLayout->addWidget(makeInfoRow(tr("🧴 B"), m_doseB));
    infoLayout->addStretch();

    auto *statusBox = new QGroupBox(tr("Показания"), this);
    statusBox->setLayout(infoLayout);

    auto *buttonsLayout = new QHBoxLayout;
    auto *startButton = new QPushButton(tr("▶️ Подготовить компот"), this);
    auto *stopButton = new QPushButton(tr("⏹️ Остановить процесс"), this);
    auto *readButton = new QPushButton(tr("🔄 Прочитать данные"), this);
    auto *mixButton = new QPushButton(tr("🧪 Только перемешивание"), this);

    buttonsLayout->addWidget(startButton);
    buttonsLayout->addWidget(stopButton);
    buttonsLayout->addWidget(readButton);
    buttonsLayout->addWidget(mixButton);
    buttonsLayout->addStretch();

    connect(startButton, &QPushButton::clicked, this, &ProcessView::onStartClicked);
    connect(stopButton, &QPushButton::clicked, this, &ProcessView::onAbortClicked);
    connect(readButton, &QPushButton::clicked, this, &ProcessView::onReadNowClicked);
    connect(mixButton, &QPushButton::clicked, this, &ProcessView::onMixOnlyClicked);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(inputsBox);
    layout->addWidget(statusBox);
    layout->addWidget(m_progress);
    layout->addLayout(buttonsLayout);
    layout->addWidget(m_log, 1);
}

RecipeProfile ProcessView::profile() const
{
    RecipeProfile profile;
    profile.batchLiters = m_batchLiters->value();
    profile.targetPh = m_targetPh->value();
    profile.fertAMlPerL = m_fertA->value();
    profile.fertBMlPerL = m_fertB->value();
    profile.temperature = m_temperature->value();
    return profile;
}

void ProcessView::setProfile(const RecipeProfile &profile)
{
    m_batchLiters->setValue(profile.batchLiters);
    m_targetPh->setValue(profile.targetPh);
    m_fertA->setValue(profile.fertAMlPerL);
    m_fertB->setValue(profile.fertBMlPerL);
    m_temperature->setValue(profile.temperature);
}

void ProcessView::applyStatus(const StatusFrame &frame)
{
    m_currentPh->setText(Utils::formatDouble(frame.ph));
    m_targetPhLabel->setText(Utils::formatDouble(frame.targetPh));
    m_deltaPh->setText(Utils::formatDouble(frame.deltaPh));
    m_ec->setText(Utils::formatDouble(frame.ec));
    m_tds->setText(Utils::formatDouble(frame.tds, 0));
    m_temp->setText(Utils::formatDouble(frame.temperature, 1));
    m_state->setText(Utils::stateDisplayName(frame.state));
    m_state->setStyleSheet(QStringLiteral("color: %1").arg(Utils::stateColor(frame.state).name()));
    m_doseUp->setText(QStringLiteral("%1 ml").arg(Utils::formatDouble(frame.doses.phUpMl)));
    m_doseDown->setText(QStringLiteral("%1 ml").arg(Utils::formatDouble(frame.doses.phDownMl)));
    m_doseA->setText(QStringLiteral("%1 ml").arg(Utils::formatDouble(frame.doses.fertAMl)));
    m_doseB->setText(QStringLiteral("%1 ml").arg(Utils::formatDouble(frame.doses.fertBMl)));

    updateProgress(frame);

    if (frame.state != m_lastState) {
        m_log->append(QStringLiteral("[%1] STATE %2")
                          .arg(frame.timestamp.toString(Qt::ISODate))
                          .arg(Utils::stateDisplayName(frame.state)));
        m_lastState = frame.state;
    }
}

void ProcessView::handleFault(const StatusFrame &frame)
{
    const QString message = tr("FAULT %1 %2").arg(frame.faultCode).arg(frame.faultMessage);
    m_log->append(QStringLiteral("[%1] %2")
                      .arg(frame.timestamp.toString(Qt::ISODate))
                      .arg(message));
}

void ProcessView::onStartClicked()
{
    emit startRequested(profile());
}

void ProcessView::onAbortClicked()
{
    emit abortRequested(tr("operator"));
}

void ProcessView::onReadNowClicked()
{
    emit readNowRequested();
}

void ProcessView::onMixOnlyClicked()
{
    emit mixOnlyRequested(60000);
}

void ProcessView::updateProgress(const StatusFrame &frame)
{
    int value = 0;
    switch (frame.state) {
    case ProcessState::Idle:
        value = 0;
        break;
    case ProcessState::Mix:
        value = 10;
        break;
    case ProcessState::PhCoarse:
        value = 25;
        break;
    case ProcessState::PhFine:
        value = 45;
        break;
    case ProcessState::FertA:
        value = 65;
        break;
    case ProcessState::FertB:
        value = 80;
        break;
    case ProcessState::Done:
        value = 100;
        break;
    case ProcessState::Fault:
        value = 0;
        break;
    }
    m_progress->setValue(value);
}
