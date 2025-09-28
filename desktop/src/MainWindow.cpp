#include "MainWindow.h"

#include "DeviceTab.h"

#include <QAction>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabWidget>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new Settings(this))
    , m_tabs(new QTabWidget(this))
{
    setWindowTitle(tr("SunKiss Controller"));
    setCentralWidget(m_tabs);

    auto *fileMenu = menuBar()->addMenu(tr("Файл"));
    auto *addAction = fileMenu->addAction(tr("Добавить устройство"));
    auto *saveAction = fileMenu->addAction(tr("Сохранить настройки"));
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(tr("Выход"));

    connect(addAction, &QAction::triggered, this, &MainWindow::onAddDevice);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveSettings);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    loadSettings();
    rebuildTabs();
}

MainWindow::~MainWindow() = default;

void MainWindow::loadSettings()
{
    m_settings->load();
    if (m_settings->devices().isEmpty()) {
        DeviceBinding binding;
        binding.displayName = tr("Устройство 1");
        m_settings->devices().append(binding);
    }
}

void MainWindow::rebuildTabs()
{
    m_tabs->clear();
    for (int i = 0; i < m_settings->devices().size(); ++i) {
        DeviceBinding &binding = m_settings->devices()[i];
        auto *tab = new DeviceTab(m_settings, this);
        tab->bindDevice(binding);
        connect(tab, &DeviceTab::requestSave, this, &MainWindow::onSaveSettings);
        m_tabs->addTab(tab, binding.displayName.isEmpty() ? tr("Устройство %1").arg(i + 1) : binding.displayName);
    }
}

void MainWindow::onAddDevice()
{
    DeviceBinding binding;
    binding.displayName = tr("Устройство %1").arg(m_settings->devices().size() + 1);
    m_settings->devices().append(binding);
    rebuildTabs();
}

void MainWindow::onSaveSettings()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<DeviceTab *>(m_tabs->widget(i));
        if (!tab) {
            continue;
        }
        m_settings->devices()[i] = tab->binding();
    }
    m_settings->save();
    statusBar()->showMessage(tr("Настройки сохранены"), 3000);
}
