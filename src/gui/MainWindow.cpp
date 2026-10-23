#include "gui/widgets/MeasurePage.h"
#include "gui/widgets/PilotagePage.h"
#include "gui/widgets/DebugPage.h"
#include "gui/components/TwoStateButton.h"
#include "gui/MainWindow.h"
#include "serial/Protocol.h"
#include "core/AppSettings.h"
#include <QFileInfo>
#include "gui/widgets/SerialWidget.h"
#include "gui/components/StyleHelpers.h"
#include "gui/widgets/StatusWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QApplication>
#include <QTabBar>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QCloseEvent>
#include <QShortcut>
#include "gui/widgets/VisualisationPage.h"


MainWindow::MainWindow(QWidget *parent, SerialController *serial) : QMainWindow(parent) {
    setWindowTitle("Porte Blum"); resize(1200, 760);
    serial_ = serial ? serial : new SerialController(this);

    auto central = new QWidget; setCentralWidget(central);
    auto mainLayout = new QVBoxLayout(central);
    mainLayout->addWidget(createTab(), 1);
    mainLayout->addWidget(createFooter());

    // Connect the serial controller's signals to update the UI
    connect(serial_, &SerialController::commandFinished, this, &MainWindow::dispatchEvents);
    connect(serial_, &SerialController::portsChanged, this, &MainWindow::updateSerialData);
    connect(serial_, &SerialController::systemStateChanged, this, &MainWindow::updateSerialData);
    connect(serial_, &SerialController::connectionChanged, this, [this](bool, const QString &) {
        statusWidget_->setUnknown(); updateSerialData();
    });
    connect(serial_, &SerialController::fault, this, [this](const QString &text) {
        statusWidget_->setError(text);
    });
    connect(serial_, &SerialController::deviceError, this, [this](const QString &message) {
        showError(tr("Erreur carte"), message);
        statusWidget_->setError(message);
    });
    connect(serial_, &SerialController::connectionLost, this, [this](const QString &message) {
        showError(tr("Connexion série perdue"), message);
        statusWidget_->setError(message);
    });
    connect(serial_, &SerialController::deviceWarning, this, [this](const QString &text) {
        statusWidget_->setWarning(text);
    });
    connect(serial_, &SerialController::resetErrors, statusWidget_, &StatusWidget::reset);
    // Timer to periodically dispatch pending events and measurements to the active page.
    connect(&poll_, &QTimer::timeout, this, [this] {dispatchEvents();});
    poll_.start(50); 
    updateSerialData();
    // Restore the main window geometry from settings, and save it on exit.
    QSettings s; restoreGeometry(s.value("window/geometry").toByteArray());
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { QSettings().setValue("window/geometry", saveGeometry()); });
}

MainWindow::~MainWindow() {
    const auto windows = visualisationWindows_;
    for (auto window : windows) delete window;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QMainWindow::closeEvent(event);
    if (!event->isAccepted()) return;
    const auto windows = visualisationWindows_;
    // Close all visualisation windows before the main window, to avoid dangling references.
    for (auto window : windows) window->close();
}

void MainWindow::dispatchEvents() {
    const auto events = serial_->queues()->events.since(eventCursor_);
    eventCursor_ = events.cursor;
    for (auto page : pages_) {
        page->processEvents(events);
    }
    dispatchMesures();
}

void MainWindow::dispatchMesures() {
    const auto measures = serial_->queues()->measures.since(measureCursor_);
    measureCursor_ = measures.cursor;
    if (activePage_ != pages_[Debug]) activePage_->processMeasurements(measures);
    pages_[Debug]->processMeasurements(measures);
}



void MainWindow::connectPage(Page *page) {
    //connect(page, &Page::sendCommand, this, &MainWindow::dispatchPendingData);
    connect(page, &Page::sendCommand, serial_, &SerialController::sendCommand);
    connect(page, &Page::sendRawCommand, serial_, &SerialController::sendRawCommand);
    connect(serial_, &SerialController::systemStateChanged, page, &Page::setSystemState);
    connect(serial_, &SerialController::commandFinished, page, &Page::commandFinished);
    connect(page, &Page::readyToExit, this, &MainWindow::onPageReadyToExit);
    page->setSystemState(serial_->systemState());
}

void MainWindow::onPageReadyToExit(bool success) {
    auto page = qobject_cast<Page *>(sender());
    if (!pendingPage_ || page != controlPage_) return;
    auto target = pendingPage_;
    pendingPage_ = nullptr;
    tabs_->tabBar()->setEnabled(true);
    if (!success) {
        statusWidget_->setError(tr("Arrêt non confirmé : le changement d’onglet est annulé."));
        return;
    }
    dispatchEvents();
    controlPage_ = activePage_ = target;
    const QSignalBlocker blocker(tabs_);
    tabs_->setCurrentWidget(target);
}

void MainWindow::showError(const QString &title, const QString &message) {
    auto text = message.trimmed();
    if (!text.isEmpty()) text.replace(0, 1, text.left(1).toUpper());
    auto dialog = new QMessageBox(QMessageBox::Critical, title, text, QMessageBox::Ok, this);
    dialog->setObjectName("errorDialog");
    dialog->setTextFormat(Qt::PlainText);
    dialog->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->open();
}

// Update the connection button and reset button based on the serial controller's state.
void MainWindow::updateSerialData() {
    const QSignalBlocker blocker(connection_);
    connection_->setChecked(serial_->busy());
    reset_->setEnabled(serial_->systemState().canSendCommands());
}

// Create the main tab widget with Measures, Pilotage, and Debug pages.
QWidget *MainWindow::createTab() {
    tabs_ = new QTabWidget;
    tabs_->setElideMode(Qt::ElideNone);
    // Keep the full tab bar in the layout's minimum width, including Debug.
    tabs_->setUsesScrollButtons(false);
    // Create the three main pages: Measures, Pilotage, and Debug (hidden by default).
    auto measures = new MeasurePage(this);
    tabs_->addTab(measures, "Mesures");
    auto pilotage = new PilotagePage(this);
    tabs_->addTab(pilotage, "Pilotage");
    controlPage_ = measures;
    activePage_ = measures;
    const int controlsWidth = qMax(measures->controlsWidth(), pilotage->controlsWidth());
    measures->setControlsWidth(controlsWidth);
    pilotage->setControlsWidth(controlsWidth);
    auto debug = new DebugPage(tabs_);
    debug->hide();
    
    pages_ = {measures, pilotage, debug};
    // Gestion des raccourcis clavier pour basculer entre les onglets et déclencher l'action principale.
    auto mesureShortcut = new QShortcut(QKeySequence("Ctrl+M"), this);
    mesureShortcut->setAutoRepeat(false);
    connect(mesureShortcut, &QShortcut::activated, this, &MainWindow::showMesure);
    auto pilotageShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);
    pilotageShortcut->setAutoRepeat(false);
    connect(pilotageShortcut, &QShortcut::activated, this, &MainWindow::showPilotage);
    auto debugShortcut = new QShortcut(QKeySequence("Ctrl+D"), this);
    debugShortcut->setAutoRepeat(false);
    connect(debugShortcut, &QShortcut::activated, this, &MainWindow::showDebug);
    for (auto page : pages_) connectPage(page);
    
    // Logo and connection buttons in the tab bar corners.
    auto logo = new QPushButton; logo->setObjectName("logo");
    logo->setFocusPolicy(Qt::NoFocus);
    auto logoContainer = new QWidget; 
    auto logoLayout = new QVBoxLayout(logoContainer);
    logoLayout->setContentsMargins(15, 0, 10, 5); 
    logoLayout->addWidget(logo);
    tabs_->setCornerWidget(logoContainer, Qt::TopLeftCorner);

    // Connection and load buttons in the top right corner of the tab bar.
    auto actions = new QWidget; 
    auto bar = new QHBoxLayout(actions); 
    bar->setContentsMargins(0, 0, 5, 4); 
    bar->setSpacing(5);
    connection_ = new TwoStateButton;
    connection_->setObjectName("boutonRunStop");
    connection_->setShortcut(QKeySequence("Ctrl+A"));
    connection_->setStateText(false, tr("Connecter"));
    connection_->setStateIcon(false, QIcon(":/icons/connect.svg"));
    connection_->setStateToolTip(false, tr("Connecter le port s\u00e9rie"));
    connection_->setStateText(true, tr("D\u00e9connecter"));
    connection_->setStateIcon(true, QIcon(":/icons/disconnect.svg"));
    connection_->setStateToolTip(true, tr("Déconnecter le port s\u00e9rie"));
    auto load = new QPushButton("Ouvrir"); 
    load->setObjectName("boutonOuvrir");
    load->setShortcut(QKeySequence("Ctrl+O"));
    load->setIcon(QIcon(":/icons/open.svg"));
    load->setToolTip(tr("Ouvrir des mesures enregistrées"));
    connect(load, &QPushButton::clicked, this, &MainWindow::requestOpen);
    bar->addWidget(connection_); 
    bar->addWidget(load); 
    tabs_->setCornerWidget(actions, Qt::TopRightCorner);
    connect(connection_, &QPushButton::toggled, this, &MainWindow::toggleSerialConnection);
    connect(logo, &QPushButton::clicked, this, &MainWindow::toogleDebug);
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::toogleTab);
    return tabs_;
}

void MainWindow::showMesure() {
    if (pendingPage_ || !tabs_->tabBar()->isEnabled()) return;
    if (tabs_->currentIndex() != Measures)
        tabs_->setCurrentIndex(Measures);
    else if (auto page = qobject_cast<MeasurePage *>(tabs_->currentWidget()))
        page->triggerPrimaryAction();
}

void MainWindow::showDebug() {
    if (!debugVisible_ || pendingPage_ || !tabs_->tabBar()->isEnabled()) return;
    if (tabs_->currentIndex() != Debug)
        tabs_->setCurrentIndex(Debug);
    else if (auto page = qobject_cast<DebugPage *>(tabs_->currentWidget()))
        page->triggerPrimaryAction();
}

void MainWindow::showPilotage() {
    if (pendingPage_ || !tabs_->tabBar()->isEnabled()) return;
    if (tabs_->currentIndex() != Pilotage)
        tabs_->setCurrentIndex(Pilotage);
    else if (auto page = qobject_cast<PilotagePage *>(tabs_->currentWidget()))
        page->triggerPrimaryAction();
}

void MainWindow::requestOpen() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Ouvrir des mesures"), AppSettings::instance().openFolder(), tr("Données CSV (*.csv)"));
    if (path.isEmpty()) return;
    QString configError;
    if (!AppSettings::instance().save({{"files/openFolder", QFileInfo(path).absolutePath()}}, &configError))
        showError(tr("Configuration"), configError);
    // Open a new visualisation window for the selected CSV file.
    auto window = new VisuPage;
    QString error;
    if (!window->load(path, &error)) {
        delete window;
        showError(tr("Ouverture des mesures"), error);
        return;
    }
    // Keep track of the visualisation window and remove it from the set when it's destroyed.
    visualisationWindows_.insert(window);
    connect(window, &QObject::destroyed, this, [this, window] { visualisationWindows_.remove(window);});
    window->show();
}

void MainWindow::toggleSerialConnection(bool on) {
    const QSignalBlocker blocker(connection_);
    connection_->setChecked(serial_->busy());
    if (!on) {
        serial_->disconnectPort();
        updateSerialData();
        return;
    }
    if (!serial_->busy() && serial_->selectedPort().isEmpty()) {
        const auto message = tr("Aucune carte détectée. Vérifiez le branchement USB et les paramètres du port série.");
        showError(tr("Connexion impossible"), message);
        return;
    }
    serial_->connectPort();
    updateSerialData();
}

void MainWindow::toogleDebug() {
    auto debug = pages_[Debug];
    // Triple-click the logo within one second to toggle the debug page.
    if (!clicksTime_.isValid() || clicksTime_.elapsed() > 1000) { 
        clicks_ = 0; clicksTime_.restart(); 
    }
    if (++clicks_ != 3) return;
    // Reset the click counter and toggle the debug mode.
    clicks_ = 0; debugVisible_ = !debugVisible_;
    serialWidget_->setDebugMode(debugVisible_);
    for (auto page : pages_) {
        page->setDebugMode(debugVisible_);
    }
    // Show or hide the debug tab based on the debugVisible_ flag.
    if (debugVisible_) { 
        tabs_->addTab(debug, "Debug"); }
    else { 
        tabs_->removeTab(tabs_->indexOf(debug)); debug->hide(); 
    }
}

void MainWindow::toogleTab() {
    auto current = qobject_cast<Page *>(tabs_->currentWidget());
    if (pendingPage_) {
        const QSignalBlocker blocker(tabs_);
        tabs_->setCurrentWidget(controlPage_);
    } else {
        dispatchEvents();
        activePage_ = current;
        if (current && current != pages_[Debug] && current != controlPage_) {
            // Restore the tab while waiting for the current page to stop.
            {
                const QSignalBlocker blocker(tabs_);
                tabs_->setCurrentWidget(controlPage_);
            }
            activePage_ = controlPage_;
            pendingPage_ = current;
            tabs_->tabBar()->setEnabled(false);
            controlPage_->requestExit();
        }
    }
}

// Footer with serial connection status, indicator, and reset button.
QWidget *MainWindow::createFooter() {
    auto footer = new QWidget;
    footer->setObjectName("footer");
    auto layout = new QHBoxLayout(footer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // SerialWidget with port selection and connection status.
    serialWidget_ = new SerialWidget(serial_);
    layout->addWidget(serialWidget_);

    // Message and indicator share error/warning priority management.
    statusWidget_ = new StatusWidget;
    statusWidget_->setObjectName("statusWidget");
    layout->addWidget(statusWidget_, 1);

    // Reset button to send a reset command to the Arduino.
    reset_ = new QPushButton("R"); 
    reset_->setObjectName("boutonReset"); 
    reset_->setShortcut(QKeySequence("Ctrl+R"));
    reset_->setAccessibleName(tr("Réinitialiser la carte Arduino")); 
    reset_->setToolTip("Reset de la carte Arduino"); 
    layout->addWidget(reset_);
    reset_->setFocusPolicy(Qt::NoFocus);
    statusWidget_->setUnknown();
    connect(reset_, &QPushButton::clicked, serial_, [this] { serial_->sendCommand(Protocol::CMD::Reset); });
    
    return footer;
}
