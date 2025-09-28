#pragma once

#include "RecipeProfile.h"
#include "StatusFrame.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QTextEdit;

class ProcessView : public QWidget
{
    Q_OBJECT
public:
    explicit ProcessView(QWidget *parent = nullptr);

    RecipeProfile profile() const;
    void setProfile(const RecipeProfile &profile);

signals:
    void startRequested(const RecipeProfile &profile);
    void abortRequested(const QString &reason);
    void readNowRequested();
    void mixOnlyRequested(int durationMs);

public slots:
    void applyStatus(const StatusFrame &frame);
    void handleFault(const StatusFrame &frame);

private slots:
    void onStartClicked();
    void onAbortClicked();
    void onReadNowClicked();
    void onMixOnlyClicked();

private:
    void updateProgress(const StatusFrame &frame);

    QDoubleSpinBox *m_batchLiters;
    QDoubleSpinBox *m_targetPh;
    QDoubleSpinBox *m_fertA;
    QDoubleSpinBox *m_fertB;
    QDoubleSpinBox *m_temperature;

    QLabel *m_currentPh;
    QLabel *m_targetPhLabel;
    QLabel *m_deltaPh;
    QLabel *m_ec;
    QLabel *m_tds;
    QLabel *m_temp;
    QLabel *m_state;
    QLabel *m_doseUp;
    QLabel *m_doseDown;
    QLabel *m_doseA;
    QLabel *m_doseB;

    QProgressBar *m_progress;
    QTextEdit *m_log;
    ProcessState m_lastState = ProcessState::Idle;
};
