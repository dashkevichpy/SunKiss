#pragma once

#include "Settings.h"

#include <QMainWindow>

class DeviceTab;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAddDevice();
    void onSaveSettings();

private:
    void loadSettings();
    void rebuildTabs();

    Settings *m_settings;
    QTabWidget *m_tabs;
};
