#include "core/AppSettings.h"
#include "gui/components/TwoStateButton.h"
#include "gui/components/CalibrationButton.h"
#include "gui/components/StatusIndicator.h"
#include "gui/widgets/StatusWidget.h"
#include <QLocale>
#include <QtTest>
#include <cmath>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSettings>
#include <QProcess>
#include <QSharedMemory>
#include <QUuid>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextFragment>
#include <QFontMetricsF>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTranslator>
#include <QDialogButtonBox>
#include <QtEndian>
#include "gui/MainWindow.h"
#include "gui/widgets/MeasurePage.h"
#include "gui/widgets/DebugPage.h"
#include "gui/widgets/SerialWidget.h"
#include <QDialog>
#include "serial/Protocol.h"
#include "gui/widgets/StateGraphWidget.h"
#include "gui/widgets/GraphWidget.h"
#include "gui/widgets/VisualisationPage.h"
#include "gui/widgets/PilotagePage.h"
#include <QProgressDialog>
#include <QScrollArea>
#include <QScrollBar>
#include "gui/widgets/ConsigneWidget.h"
#include "gui/widgets/PIDWidget.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include "gui/components/ColorCheckBox.h"
#include <QCheckBox>
#include <QGroupBox>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#endif
class ReceivingPage final : public Page {
public:
    int states = 0, replies = 0;
    bool accepted = true;
    Protocol::CMD reply = Protocol::CMD::Reset;
    void setSystemState(const SystemState &state) override { ++states; Page::setSystemState(state); }
    void commandFinished(Protocol::CMD command, bool ok) override {
        ++replies; reply = command; accepted = ok;
    }
};

class IntegrationTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
private slots:
    void splashRunsInSeparateProcess() {
        QSharedMemory state("splash-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        QVERIFY(state.create(1));
        QVERIFY(state.lock());
        *static_cast<char *>(state.data()) = 0;
        state.unlock();
        QString executable = QCoreApplication::applicationDirPath() + "/SoftPorteBlum";
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        QProcess splash;
        splash.start(executable, {"--startup-splash", state.key()});
        QVERIFY(splash.waitForStarted(5000));
        QVERIFY2(splash.waitForReadyRead(5000), qPrintable(QString::fromUtf8(splash.readAllStandardError())));
        QCOMPARE(splash.readAllStandardOutput().trimmed(), QByteArray("ready"));
        // Deliberately block the parent's GUI thread: the helper keeps its own event loop.
        QThread::msleep(900);
        QVERIFY(!splash.waitForFinished(20));
        QVERIFY(state.lock());
        *static_cast<char *>(state.data()) = 1;
        state.unlock();
        QVERIFY(splash.waitForFinished(3000));
        QCOMPARE(splash.exitStatus(), QProcess::NormalExit);
        QCOMPARE(splash.exitCode(), 0);

        // Startup can finish before the helper has opened its window.
        splash.start(executable, {"--startup-splash", state.key()});
        QVERIFY(splash.waitForStarted(5000));
        QVERIFY(splash.waitForFinished(3000));
        QCOMPARE(splash.exitCode(), 0);

        splash.start(executable, {"--startup-splash", "missing-" + state.key()});
        QVERIFY(splash.waitForStarted(5000));
        QVERIFY(splash.waitForFinished(3000));
        QCOMPARE(splash.exitCode(), 2);
    }

    void mainWindowKeyboardShortcuts() {
        MainWindow window;
        window.show();
        window.activateWindow();
        QTest::qWait(50);
        auto tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        auto measures = tabs->widget(0);
        auto pilotage = tabs->widget(1);
        auto measureRun = measures->findChild<QPushButton *>("boutonRunStop");
        auto pilotRun = pilotage->findChild<QPushButton *>("boutonRunStop");
        QVERIFY(measureRun && pilotRun);
        // Observe the real keyboard routing without sending hardware commands.
        measureRun->disconnect();
        pilotRun->disconnect();
        measureRun->setEnabled(true);
        pilotRun->setEnabled(true);
        QSignalSpy measureClicks(measureRun, &QPushButton::clicked);
        QSignalSpy pilotClicks(pilotRun, &QPushButton::clicked);
        QTest::keyClick(&window, Qt::Key_P, Qt::ControlModifier);
        QCOMPARE(tabs->currentWidget(), pilotage);
        QCOMPARE(pilotClicks.count(), 0);
        QTest::keyClick(&window, Qt::Key_P, Qt::ControlModifier);
        QTRY_COMPARE(pilotClicks.count(), 1);
        QVERIFY(pilotRun->isChecked());
        QTest::keyClick(&window, Qt::Key_P, Qt::ControlModifier);
        QTRY_COMPARE(pilotClicks.count(), 2);
        QVERIFY(!pilotRun->isChecked());
        QTest::keyClick(&window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(tabs->currentWidget(), measures);
        QCOMPARE(measureClicks.count(), 0);
        measureRun->setEnabled(true);
        QTest::keyClick(&window, Qt::Key_M, Qt::ControlModifier);
        QTRY_COMPARE(measureClicks.count(), 1);
        QTest::keyClick(&window, Qt::Key_M, Qt::ControlModifier);
        QTRY_VERIFY(!measureRun->isChecked());
        QTest::keyClick(&window, Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(tabs->currentWidget(), measures); // Debug is hidden.
        measureRun->setEnabled(false);
        QTest::keyClick(&window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(measureClicks.count(), 2);
        auto logo = window.findChild<QPushButton *>("logo");
        QVERIFY(logo);
        for (int i = 0; i < 3; ++i) logo->click();
        QTest::keyClick(&window, Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(tabs->currentIndex(), 2);
        const auto debugViews = tabs->currentWidget()->findChildren<QTextEdit *>();
        QVERIFY(!debugViews.isEmpty());
        for (auto view : debugViews) view->setPlainText("test");
        QTest::keyClick(&window, Qt::Key_D, Qt::ControlModifier);
        for (auto view : debugViews) QVERIFY(view->toPlainText().isEmpty());
        QTest::keyClick(&window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(tabs->currentWidget(), measures);
        QCOMPARE(measureClicks.count(), 2); // Selecting the tab does not run it.

        for (const char *name : {"boutonCalibration", "boutonSauvegarder"}) {
            auto visible = measures->findChild<QPushButton *>(name);
            auto hidden = pilotage->findChild<QPushButton *>(name);
            QVERIFY(visible && hidden);
            visible->disconnect();
            hidden->disconnect();
            visible->setEnabled(true);
            hidden->setEnabled(true);
            QSignalSpy visibleClicks(visible, &QPushButton::clicked);
            QSignalSpy hiddenClicks(hidden, &QPushButton::clicked);
            const auto key = QString::fromLatin1(name) == "boutonCalibration" ? Qt::Key_C : Qt::Key_S;
            QTest::keyClick(&window, key, Qt::ControlModifier);
            QTRY_COMPARE(visibleClicks.count(), 1);
            QCOMPARE(hiddenClicks.count(), 0);
            visible->setEnabled(false);
            QTest::keyClick(&window, key, Qt::ControlModifier);
            QCOMPARE(visibleClicks.count(), 1);
        }
    }

    void statusWidgetPriority() {
        StatusWidget widget;
        auto indicator = widget.findChild<StatusIndicator *>("statusSecurite");
        auto message = widget.findChild<QLabel *>("statusSecuriteMessage");
        QVERIFY(indicator);
        QVERIFY(message);
        QCOMPARE(indicator->state(), StatusIndicator::State::Ok);
        widget.setWarning("Premier avertissement");
        QCOMPARE(indicator->state(), StatusIndicator::State::Warning);
        widget.setError("Erreur prioritaire");
        widget.setWarning("Dernier avertissement");
        QCOMPARE(widget.lastError(), QString("Erreur prioritaire"));
        QCOMPARE(widget.lastWarning(), QString("Dernier avertissement"));
        QCOMPARE(message->text(), QString("Erreur prioritaire"));
        QCOMPARE(indicator->state(), StatusIndicator::State::Error);
        widget.resetError();
        QCOMPARE(message->text(), QString("Dernier avertissement"));
        QCOMPARE(indicator->state(), StatusIndicator::State::Warning);
        widget.setError("Nouvelle erreur");
        widget.resetWarning();
        QCOMPARE(message->text(), QString("Nouvelle erreur"));
        QCOMPARE(indicator->state(), StatusIndicator::State::Error);
        widget.resetError();
        QCOMPARE(indicator->state(), StatusIndicator::State::Ok);
        QVERIFY(message->isHidden());
        widget.setWarning("Attention");
        widget.setError("Erreur");
        widget.reset();
        QVERIFY(widget.lastError().isEmpty());
        QVERIFY(widget.lastWarning().isEmpty());
        QVERIFY(message->text().isEmpty());
        QCOMPARE(indicator->state(), StatusIndicator::State::Ok);
        widget.setUnknown();
        QVERIFY(!indicator->isEnabled());
        widget.setError("Erreur après déconnexion");
        QVERIFY(indicator->isEnabled());
        QCOMPARE(indicator->state(), StatusIndicator::State::Error);
    }

    void statusIndicatorStyle() {
        StatusIndicator indicator;
        QFile style(":/styles.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        indicator.setStyleSheet(QString::fromUtf8(style.readAll()));
        const auto centerColor = [&indicator] {
            QImage image(indicator.size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            indicator.render(&image);
            return image.pixelColor(image.width() / 2, image.height() / 2);
        };
        QCOMPARE(centerColor(), QColor("#DC2626"));
        indicator.setState(StatusIndicator::State::Warning);
        QCOMPARE(centerColor(), QColor("#F75A1B"));
        indicator.setState(StatusIndicator::State::Ok);
        QCOMPARE(centerColor(), QColor("#16A34A"));
        indicator.setEnabled(false);
        QCOMPARE(centerColor(), QColor("#6B7280"));
        indicator.setState(StatusIndicator::State::Warning);
        QCOMPARE(centerColor(), QColor("#6B7280"));
        indicator.setEnabled(true);
        QCOMPARE(centerColor(), QColor("#F75A1B"));
        indicator.setStyleSheet("StatusIndicator[state=\"warning\"] { color: #123456; }");
        QCOMPARE(centerColor(), QColor("#123456"));
    }

    void widgetDebugInformation() {
        StateGraphWidget states;
        auto production = states.findChild<QLabel *>("productionState");
        QVERIFY(production);
        QVERIFY(production->parentWidget()->isHidden());
        states.setState(250, 3);
        states.setDebugMode(true);
        QVERIFY(!production->parentWidget()->isHidden());
        QVERIFY(production->text().startsWith("Production\n3 :"));
        states.setDebugMode(false);
        QVERIFY(production->parentWidget()->isHidden());
        GraphWidget graph;
        auto summary = graph.findChild<QLabel *>("measurementSummary");
        QVERIFY(summary && summary->isHidden());
        MeasurementStore store;
        store.lost = 7;
        graph.refresh(store);
        graph.setDebugMode(true);
        QVERIFY(!summary->isHidden());
        QVERIFY(summary->text().contains("7"));
        QVERIFY(!summary->text().contains('\n'));
        graph.setDebugMode(false);
        QVERIFY(summary->isHidden());
    }
    void automaticSerialBaud() {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.ini");
        const auto &settings = AppSettings::instance(path);
        QVERIFY(settings.save({{"serial/automatic", true}, {"serial/baud", 9600}}));
        SerialController serial(nullptr, path);
        const int defaultBaud = AppSettings::defaultValue("serial/baud").toInt();
        QCOMPARE(serial.baud, defaultBaud);
        SerialWidget widget(&serial);
        widget.setDebugMode(true);
        bool inspected = false, locked = false, unlocked = false;
        QTimer::singleShot(0, &widget, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto automatic = dialog->findChild<QCheckBox *>();
            const auto combos = dialog->findChildren<QComboBox *>();
            if (automatic && combos.size() == 2) {
                inspected = true;
                locked = !combos[0]->isEnabled() && !combos[1]->isEnabled()
                    && combos[1]->currentText().toInt() == defaultBaud;
                automatic->setChecked(false);
                unlocked = combos[0]->isEnabled() && combos[1]->isEnabled();
                combos[1]->setCurrentText("57600");
                automatic->setChecked(true);
                locked = locked && !combos[1]->isEnabled()
                    && combos[1]->currentText().toInt() == defaultBaud;
            }
            dialog->reject();
        });
        widget.findChild<QPushButton *>("labelPort")->click();
        QVERIFY(inspected && locked && unlocked);
        serial.baud = 57600;
        QVERIFY(serial.saveSettings());
        QCOMPARE(serial.baud, defaultBaud);
        QVERIFY(settings.save({{"serial/automatic", false}, {"serial/baud", 9600}}));
        SerialController manual(nullptr, path);
        QCOMPARE(manual.baud, 9600);
    }
    void serialWidgetControllerAndDialog() {
        QTemporaryDir dir;
        SerialController serial(nullptr, dir.filePath("config.ini"));
        QString error;
        QVERIFY(serial.applySerialSettings(false, "SoftPorteBlum_OLD_INVALID", 57600, &error));
        SerialWidget widget(&serial);
        auto indicator = widget.findChild<StatusIndicator *>("serialIndicator");
        auto status = widget.findChild<QPushButton *>("labelPort");
        QVERIFY(indicator);
        QVERIFY(status);
        QVERIFY(!status->isHidden());
        QCOMPARE(widget.currentState(), SerialWidget::ConnectionState::Disconnected);
        QCOMPARE(widget.property("state").toString(), QString("disconnected"));
        QVERIFY(status->text().contains(serial.selectedPort()));
        widget.setDebugMode(true);
        QTimer::singleShot(0, &widget, [&] {
            auto dialog = widget.findChild<QDialog *>("serialSettingsDialog");
            if (!dialog) return;
            const auto combos = dialog->findChildren<QComboBox *>();
            if (combos.size() != 2) { dialog->reject(); return; }
            combos[0]->setCurrentText("SoftPorteBlum_NEW_INVALID");
            combos[1]->setCurrentText("115200");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        widget.show();
        QTest::mouseClick(status, Qt::LeftButton);
        QCOMPARE(serial.manualPort, QString("SoftPorteBlum_NEW_INVALID"));
        QCOMPARE(serial.baud, 115200);
        QVERIFY(!serial.automatic);
        QVERIFY(status->text().contains(serial.selectedPort()));
        QCOMPARE(indicator->state(), StatusIndicator::State::Error);
        serial.manualPort.clear();
        serial.scan();
        QVERIFY(!status->isHidden());
        QCOMPARE(widget.currentState(), SerialWidget::ConnectionState::Unknown);
        QCOMPARE(widget.property("state").toString(), QString("unknown"));
        QVERIFY(!indicator->isEnabled());
    }
    void serialSettingsDisconnect() {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.ini");
        const auto &settings = AppSettings::instance(path);
        QVERIFY(settings.save({{"serial/automatic", false}, {"serial/port", "SoftPorteBlum_OLD_INVALID"},
                               {"serial/baud", 57600}}));
        SerialController serial(nullptr, path);
        QSignalSpy stopped(&serial, &SerialController::stopRequested);
        serial.connectPort();
        QVERIFY(serial.busy());
        QString error;
        QVERIFY(serial.applySerialSettings(false, "SoftPorteBlum_OLD_INVALID", 57600, &error));
        QCOMPARE(stopped.size(), 0); // Sauvegarder les mêmes paramètres conserve la liaison.
        QVERIFY(serial.applySerialSettings(false, "SoftPorteBlum_NEW_INVALID", 57600, &error));
        QCOMPARE(stopped.size(), 1);
        QTRY_VERIFY(!serial.busy());
        QVERIFY(!serial.connected());
        QCOMPARE(serial.systemState().port, QString("SoftPorteBlum_NEW_INVALID"));
        QCOMPARE(settings.value("serial/port").toString(), QString("SoftPorteBlum_NEW_INVALID"));
        QVERIFY(!serial.applySerialSettings(false, QString(), 57600, &error));
        QCOMPARE(serial.systemState().port, QString("SoftPorteBlum_NEW_INVALID"));
    }

    void mainWindowSerialSettings() {
        QTemporaryDir dir;
        SerialController serial(nullptr, dir.filePath("config.ini"));
        MainWindow window(nullptr, &serial);
        auto widget = window.findChild<SerialWidget *>();
        QVERIFY(widget);
        QString error;
        QVERIFY(serial.applySerialSettings(false, "SoftPorteBlum_NEW_INVALID", 57600, &error));
        QCOMPARE(widget->currentState(), SerialWidget::ConnectionState::Disconnected);
        QVERIFY(widget->findChild<QPushButton *>("labelPort")->text().contains(serial.selectedPort()));
    }
    void consigneTableau() {
        ConsigneWidget widget;
        auto type = widget.findChild<QComboBox *>("typeConsigne");
        auto table = widget.findChild<QTableWidget *>("pointsConsigne");
        auto add = widget.findChild<QPushButton *>("ajouterPointConsigne");
        auto remove = widget.findChild<QPushButton *>("retirerPointConsigne");
        auto invert = widget.findChild<QCheckBox *>("inverserTableau");
        QVERIFY(type && table && add && remove && invert);
        type->setCurrentIndex(type->findData(QVariant::fromValue(Protocol::CMD::ConsigneTableau)));
        QCOMPARE(table->rowCount(), 2);
        table->item(0, 0)->setData(Qt::EditRole, 5.0); // La ligne passe après celle à 1 s.
        QCOMPARE(table->item(0, 0)->data(Qt::EditRole).toDouble(), 1.0);
        table->item(0, 1)->setData(Qt::EditRole, 100.0);
        table->item(1, 1)->setData(Qt::EditRole, -50.0);
        auto commands = widget.getCommand();
        QCOMPARE(commands.size(), 3);
        QCOMPARE(commands[0].text(), QString("SET CONSIGNE TABLEAU 2"));
        QCOMPARE(commands[1].text(), QString("SET CONSIGNE TABLEAU 1000.00 100.00"));
        QCOMPARE(commands[2].text(), QString("SET CONSIGNE TABLEAU 5000.00 -50.00"));
        invert->setChecked(true);
        QCOMPARE(widget.getCommand()[1].values[1], QString("-100.00"));
        QCOMPARE(widget.getCommand()[2].values[1], QString("50.00"));
        QCOMPARE(invert->text(), QString("Inverser"));
        QCOMPARE(invert->layoutDirection(), Qt::RightToLeft);
        QVERIFY(!add->icon().isNull());
        QVERIFY(!remove->icon().isNull());
        widget.setLimitesAmplitude(-10, 10, "rad/s");
        QCOMPARE(widget.getCommand()[1].values[1], QString("-10.00"));
        QCOMPARE(widget.getCommand()[2].values[1], QString("10.00"));
        table->setCurrentCell(1, 0);
        add->click();
        QCOMPARE(table->rowCount(), 3);
        QCOMPARE(table->item(2, 0)->data(Qt::EditRole).toDouble(), 6.0);
        remove->click();
        QCOMPARE(table->rowCount(), 2);
        QVERIFY(widget.description().contains("interpolation"));
        QVERIFY(!widget.findChild<QWidget *>("apercuConsigne")->grab().isNull());
        type->setCurrentIndex(0);
        QCOMPARE(widget.getCommand().size(), 1);
        type->setCurrentIndex(type->findData(QVariant::fromValue(Protocol::CMD::ConsigneTableau)));
        QCOMPARE(table->rowCount(), 2); // Les points sont conservés lors des changements de profil.
    }
    void consigneProfilesAndCommands() {
        ConsigneWidget widget;
        auto type = widget.findChild<QComboBox *>("typeConsigne");
        auto amplitude = widget.findChild<QDoubleSpinBox *>("amplitude");
        auto duration = widget.findChild<QDoubleSpinBox *>("duree");
        auto offset = widget.findChild<QDoubleSpinBox *>("offset");
        auto invert = widget.findChild<QCheckBox *>("inverserConsigne");
        amplitude->setValue(100); offset->setValue(2);
        invert->setChecked(true);
        QCOMPARE(widget.getCommand().first().text(), QString("SET CONSIGNE ECHELON -100.00 8000.00 2000.00"));
        type->setCurrentIndex(1);
        QCOMPARE(widget.getCommand().first().text(), QString("SET CONSIGNE RAMPE 0.00 -100.00 8000.00 2000.00"));
        type->setCurrentIndex(2);
        QCOMPARE(widget.getCommand().first().text(), QString("SET CONSIGNE TRAPEZE -100.00 1000.00 6000.00 1000.00 2000.00"));
        type->setCurrentIndex(3);
        auto frequency = widget.findChild<QDoubleSpinBox *>("frequence");
        frequency->setValue(2);
        QVERIFY(offset->isHidden());
        QCOMPARE(widget.getCommand().first().text(), QString("SET CONSIGNE SINUS -100.00 500.00 10000.00 0.00"));
        frequency->setValue(3);
        QCOMPARE(widget.getCommand().first().values[1], QString("333.33"));
        type->setCurrentIndex(0); offset->setValue(2);
        duration->setValue(0.1);
        QCOMPARE(offset->value(), 0.1);
        for (int i = 0; i < 4; ++i) {
            type->setCurrentIndex(i);
            QVERIFY(!widget.grab().isNull());
        }
        widget.setLimitesAmplitude(-10, 10, "rad/s");
        QCOMPARE(amplitude->value(), 10.0);
        QCOMPARE(amplitude->suffix(), QString(" rad/s"));
    }
    void consigneTestDebugMode() {
        PilotagePage page;
        auto widget = page.findChild<ConsigneWidget *>();
        QVERIFY(widget);
        auto type = widget->findChild<QComboBox *>("typeConsigne");
        QVERIFY(type);
        const auto test = QVariant::fromValue(Protocol::CMD::ConsigneTest);
        QCOMPARE(type->findData(test), -1);
        page.setDebugMode(true);
        const int index = type->findData(test);
        QVERIFY(index >= 0);
        page.setDebugMode(true);
        QCOMPARE(type->count(), 6);
        type->setCurrentIndex(index);
        const auto commands = widget->getCommand();
        QCOMPARE(commands.size(), 1);
        QVERIFY(commands.first().values.isEmpty());
        QCOMPARE(commands.first().text(), QString("SET CONSIGNE TEST"));
        QCOMPARE(widget->description(), QString("Test"));
        for (auto field : widget->findChildren<QDoubleSpinBox *>())
            QVERIFY(!field->isVisibleTo(widget));
        QVERIFY(widget->findChild<QWidget *>("apercuConsigne")->isHidden());
        page.setDebugMode(false);
        QCOMPARE(type->findData(test), -1);
        QCOMPARE(type->currentIndex(), 0);
        QCOMPARE(widget->getCommand().first().command, Protocol::CMD::ConsigneEchelon);
        QVERIFY(!widget->findChild<QWidget *>("apercuConsigne")->isHidden());
    }
    void consigneSinusOffset() {
        ConsigneWidget widget;
        auto type = widget.findChild<QComboBox *>("typeConsigne");
        auto amplitude = widget.findChild<QDoubleSpinBox *>("amplitude");
        auto delay = widget.findChild<QDoubleSpinBox *>("offset");
        auto offset = widget.findChild<QDoubleSpinBox *>("offsetSinus");
        auto preview = static_cast<ConsignePreview *>(widget.findChild<QWidget *>("apercuConsigne"));
        QVERIFY(type && amplitude && delay && offset && preview);
        widget.setLimitesAmplitude(-10, 10, "rad/s");
        amplitude->setValue(4);
        delay->setValue(2);
        type->setCurrentIndex(3);
        QVERIFY(delay->isHidden());
        QVERIFY(!offset->isHidden());
        QCOMPARE(offset->suffix(), QString(" rad/s"));
        offset->setValue(6);
        amplitude->stepUp();
        QCOMPARE(amplitude->value(), 5.0);
        QCOMPARE(offset->value(), 5.0);
        QCOMPARE(widget.getCommand().first().text(), QString("SET CONSIGNE SINUS 5.00 1000.00 10000.00 5.00"));
        QVERIFY(widget.description().contains("Offset 5"));
        auto duration = widget.findChild<QDoubleSpinBox *>("duree");
        duration->setDecimals(2);
        duration->setValue(0.25);
        duration->setValue(10);
        delay->setValue(2);
        offset->setValue(-5);
        amplitude->stepUp();
        QCOMPARE(amplitude->value(), 6.0);
        QCOMPARE(offset->value(), -4.0);
        amplitude->setValue(-7);
        QCOMPARE(offset->value(), -3.0);
        type->setCurrentIndex(0);
        QVERIFY(!delay->isHidden());
        QVERIFY(offset->isHidden());
        QCOMPARE(delay->value(), 2.0);
    }
    void twoStateButtonActions() {
        TwoStateButton button;
        button.setStateText(false, "Start");
        button.setStateText(true, "Stop operation");
        const QIcon start(":/icons/play.svg"), stop(":/icons/stop.svg");
        button.setStateIcon(false, start); button.setStateIcon(true, stop);
        QSignalSpy toggled(&button, &QPushButton::toggled);
        const auto width = button.width();
        button.click(); QVERIFY(button.state());
        QCOMPARE(toggled.size(), 1); QVERIFY(toggled.last()[0].toBool());
        QCOMPARE(button.text(), QString("Stop operation"));
        QCOMPARE(button.icon().cacheKey(), stop.cacheKey()); QCOMPARE(button.width(), width);
        button.click(); QVERIFY(!button.state());
        QCOMPARE(toggled.size(), 2); QVERIFY(!toggled.last()[0].toBool());
        QCOMPARE(button.text(), QString("Start"));
        button.setChecked(true); QCOMPARE(toggled.size(), 3);
        button.setChecked(true); QCOMPARE(toggled.size(), 3);
        button.setDisabled(true); button.click(); QCOMPARE(toggled.size(), 3);
        button.setEnabled(true); button.click(); QCOMPARE(toggled.size(), 4);
        button.show(); QTest::keyClick(&button, Qt::Key_Space);
        QVERIFY(button.state()); QCOMPARE(toggled.size(), 5);
        button.blockSignals(true); button.setChecked(false);
        QCOMPARE(toggled.size(), 5); QCOMPARE(button.text(), QString("Start"));
        button.blockSignals(false);
        QPushButton *native = &button; native->setChecked(true);
        QCOMPARE(button.text(), QString("Stop operation"));

    }
    void acquisitionButtonConfirmedState() {
        MainWindow router;
        MeasurePage page; router.connectPage(&page);
        SystemState state; state.connected = true; state.initialized = true;
        page.setSystemState(state);
        auto button = page.findChild<TwoStateButton *>("boutonRunStop"); QVERIFY(button);
        QSignalSpy sent(&page, &Page::sendCommand);
        button->click(); QVERIFY(!button->state());
        QCOMPARE(qvariant_cast<Protocol::CMD>(sent.last()[0]), Protocol::CMD::MesuresOn);
        state.measuring = true; page.setSystemState(state);
        QVERIFY(button->state());
        button->click(); QVERIFY(button->state());
        QCOMPARE(qvariant_cast<Protocol::CMD>(sent.last()[0]), Protocol::CMD::MesuresOff);
        state.measuring = false; page.setSystemState(state); QVERIFY(!button->state());
        QCOMPARE(sent.size(), 2);
    }
    void typedPageCommands() {
        MeasurePage measures;
        PilotagePage pilotage;
        SystemState ready; ready.connected = true; ready.initialized = true;
        for (Page *page : {static_cast<Page *>(&measures), static_cast<Page *>(&pilotage)}) {
            page->setSystemState(ready);
            QSignalSpy sent(page, &Page::sendCommand);
            auto calibration = page->findChild<CalibrationButton *>(); QVERIFY(calibration);
            for (bool accept : {false, true}) {
                bool shown = false, warning = false, defaultNo = false;
                QTimer::singleShot(0, [&] {
                    auto dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                    if (!dialog) return;
                    shown = true; warning = dialog->icon() == QMessageBox::Warning;
                    defaultNo = dialog->defaultButton() == dialog->button(QMessageBox::No);
                    dialog->button(accept ? QMessageBox::Yes : QMessageBox::No)->click();
                });
                calibration->click();
                QVERIFY(shown); QVERIFY(warning); QVERIFY(defaultNo);
                QCOMPARE(sent.size(), accept ? 1 : 0);
            }
            QCOMPARE(qvariant_cast<Protocol::CMD>(sent[0][0]), Protocol::CMD::Calibration);
            QVERIFY(sent[0][1].toStringList().isEmpty());
        }
        QSignalSpy commands(&pilotage, &Page::sendCommand);
        pilotage.findChild<QPushButton *>("boutonRunStop")->click();
        QCOMPARE(commands.size(), 1);
        QCOMPARE(qvariant_cast<Protocol::CMD>(commands[0][0]), Protocol::CMD::ModePWM);
        QVERIFY(commands[0][2].toBool());
        pilotage.commandFinished(Protocol::CMD::ModePWM, true);
        QCOMPARE(commands.size(), 2);
        QCOMPARE(qvariant_cast<Protocol::CMD>(commands[1][0]), pilotage.commandRequests()[1].command);
        QCOMPARE(commands[1][1].toStringList(), pilotage.commandRequests()[1].values);
        pilotage.stopPilotage();
        QSignalSpy raw(&measures, &Page::sendRawCommand);
        measures.sendRawCommand("GET CUSTOM");
        QCOMPARE(raw.size(), 1);
        QCOMPARE(raw[0][0].toString(), QString("GET CUSTOM"));
        QCOMPARE(raw[0].size(), 1);
    }
    void pilotageConfirmedCommands() {
        PilotagePage page;
        SystemState ready; ready.connected = true; ready.initialized = true;
        page.setSystemState(ready);
        const auto commands = page.commandRequests();
        auto speed = page.findChild<PIDWidget *>("pidVitesse");
        auto consigne = page.findChild<ConsigneWidget *>();
        QVERIFY(speed && consigne);
        QCOMPARE(commands.size(), 4);
        QCOMPARE(consigne->getCommand().first().text(), commands[1].text());
        QSignalSpy sent(&page, &Page::sendCommand);
        auto send = page.findChild<QPushButton *>("boutonRunStop");
        send->click();
        QTest::qWait(250);
        QCOMPARE(sent.size(), 1); // Aucune poursuite sans confirmation.
        for (qsizetype index = 0; index < commands.size(); ++index) {
            QCOMPARE(sent.size(), index + 1);
            QCOMPARE(qvariant_cast<Protocol::CMD>(sent[index][0]), commands[index].command);
            QCOMPARE(sent[index][1].toStringList(), commands[index].values);
            QVERIFY(sent[index][2].toBool());
            page.commandFinished(commands[index].command, true);
        }
        auto progress = page.findChild<QProgressDialog *>("pilotageProgress");
        QTRY_VERIFY(!progress->isVisible());
        QVERIFY(send->isChecked());
        page.stopPilotage();
    }

    void pilotageCachedMeasurement() {
        PilotagePage page;
        SystemState ready; ready.connected = true; ready.initialized = true;
        page.setSystemState(ready);
        auto graph = page.findChild<GraphWidget *>();
        QVERIFY(graph);
        EventQueue queue;
        for (quint32 time : {400u, 500u, 600u}) {
            auto measure = std::make_shared<MeasureEvent>();
            measure->time = time;
            queue.push(measure);
        }
        page.processMeasurements(queue.since(0));
        QVERIFY(graph->history().measures.empty());
        page.findChild<QPushButton *>("boutonRunStop")->click();
        for (const auto &request : page.commandRequests())
            page.commandFinished(request.command, true);
        QTRY_COMPARE(graph->history().measures.size(), size_t(1));
        QCOMPARE(static_cast<const MeasureEvent &>(*graph->history().measures.front()).time, quint32(600));
        auto time = graph->findChild<QLabel *>("graphiqueTemps");
        QCOMPARE(time->text(), QString("Temps : 0.00 s"));
        auto next = std::make_shared<MeasureEvent>(); next->time = 700;
        queue.push(next);
        page.processMeasurements(queue.since(3));
        QCOMPARE(graph->history().measures.size(), size_t(2));
        QCOMPARE(time->text(), QString("Temps : 0.10 s"));
        QSignalSpy exited(&page, &Page::readyToExit);
        auto run = page.findChild<QPushButton *>("boutonRunStop");
        connect(&page, &Page::readyToExit, &page, [run](bool success) {
            QVERIFY(success);
            QVERIFY(!run->isChecked());
        });
        page.requestExit();
        QCOMPARE(exited.size(), 1);
        next = std::make_shared<MeasureEvent>(); next->time = 800;
        queue.push(next);
        page.processMeasurements(queue.since(4));
        QCOMPARE(graph->history().measures.size(), size_t(2));
        page.findChild<QPushButton *>("boutonRunStop")->click();
        for (const auto &request : page.commandRequests())
            page.commandFinished(request.command, true);
        QTRY_COMPARE(graph->history().measures.size(), size_t(1));
        QCOMPARE(static_cast<const MeasureEvent &>(*graph->history().measures.front()).time, quint32(800));
        QCOMPARE(time->text(), QString("Temps : 0.00 s"));
        page.stopPilotage();
    }

    void pilotageConfigurationLifecycle() {
        PilotagePage page;
        SystemState ready; ready.connected = true; ready.initialized = true;
        page.setSystemState(ready);
        auto send = page.findChild<QPushButton *>("boutonRunStop");
        auto settings = page.findChild<QGroupBox *>("asservissement");
        auto consigne = page.findChild<ConsigneWidget *>();
        QVERIFY(send && settings && consigne);
        QSignalSpy sent(&page, &Page::sendCommand);
        send->click();
        auto progress = page.findChild<QProgressDialog *>("pilotageProgress");
        QVERIFY(progress && progress->isVisible());
        QVERIFY(!settings->isEnabled() && !consigne->isEnabled());
        const auto commands = page.commandRequests();
        page.commandFinished(Protocol::CMD::Stop, true);
        QCOMPARE(sent.size(), 1);
        for (qsizetype index = 0; index < commands.size(); ++index) {
            QCOMPARE(sent.size(), index + 1);
            QVERIFY(sent.last()[2].toBool());
            page.commandFinished(commands[index].command, true);
        }
        QCOMPARE(sent.size(), commands.size());
        QVERIFY(!progress->isVisible());
        QVERIFY(send->isChecked());
        page.stopPilotage();
        QVERIFY(!send->isChecked());
        QVERIFY(settings->isEnabled() && consigne->isEnabled());

        send->click();
        bool errorShown = false;
        QTimer::singleShot(0, [&] {
            auto error = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!error) return;
            errorShown = error->icon() == QMessageBox::Critical
                && error->text() == error->windowTitle()
                && error->informativeText().contains(commands.first().text());
            error->accept();
        });
        page.commandFinished(commands.first().command, false);
        QVERIFY(errorShown);
        QVERIFY(!progress->isVisible());
        QVERIFY(!send->isChecked());
        send->click();
        QVERIFY(QMetaObject::invokeMethod(progress, "canceled", Qt::DirectConnection));
        QVERIFY(!send->isChecked());
        const auto count = sent.size();
        page.commandFinished(commands.first().command, true);
        QCOMPARE(sent.size(), count); // Confirmation tardive après annulation.

        send->click();
        page.setSystemState(SystemState{});
        QVERIFY(!progress->isVisible());
        QVERIFY(!send->isEnabled() && !send->isChecked());
        const auto disconnectedCount = sent.size();
        page.commandFinished(commands.first().command, true);
        QCOMPARE(sent.size(), disconnectedCount);
    }

    void pilotageConsigneScrollKeepsActionsVisible() {
        PilotagePage page;
        auto consigne = page.findChild<ConsigneWidget *>();
        auto scroll = consigne->findChild<QScrollArea *>("consigneScroll");
        auto send = page.findChild<QPushButton *>("boutonRunStop");
        auto calibrate = page.findChild<QPushButton *>("boutonCalibration");
        QVERIFY(scroll && send && calibrate);
        consigne->findChild<QComboBox *>("typeConsigne")->setCurrentIndex(2);
        page.resize(1200, 900);
        page.show();
        QCoreApplication::processEvents();
        const int expandedHeight = consigne->height();
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        page.resize(1200, 450);
        QCoreApplication::processEvents();
        QVERIFY(consigne->height() < expandedHeight);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        for (auto button : {send, calibrate}) {
            QVERIFY(page.rect().contains(button->mapTo(&page, button->rect().bottomRight())));
            QVERIFY(button->mapTo(&page, QPoint()).y()
                >= consigne->mapTo(&page, QPoint(0, consigne->height())).y());
        }
    }

    void pilotageControlsAndMeasurements() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        MainWindow router(nullptr, &serial);
        PilotagePage widget; router.connectPage(&widget);
        QVERIFY(!widget.findChild<QObject *>("pilotageProgress"));
        auto settings = widget.findChild<QGroupBox *>("asservissement");
        auto speed = widget.findChild<PIDWidget *>("pidVitesse");
        auto position = widget.findChild<PIDWidget *>("pidPosition");
        auto graph = widget.findChild<GraphWidget *>();
        QVERIFY(settings && speed && position && graph);
        const auto requests = widget.commandRequests();
        const QStringList expectedCommands{"SET MODE PWM", "SET CONSIGNE ECHELON 255.00 10000.00 0.00", "SET MESURES ON"};
        QCOMPARE(requests.size(), expectedCommands.size());
        for (qsizetype index = 0; index < requests.size(); ++index)
            QCOMPARE(requests[index].text(), expectedCommands[index]);
        auto send = widget.findChild<QPushButton *>("boutonRunStop");
        auto calibrate = widget.findChild<QPushButton *>("boutonCalibration");
        QVERIFY(send && calibrate);
        QVERIFY(!send->isEnabled() && !calibrate->isEnabled());
        QCOMPARE(send->text(), QString("Envoyer"));
        settings->setChecked(true);
        QCOMPARE(widget.typeAsservissement(), PilotagePage::TypeAsservissement::PositionVitesse);
        QCOMPARE(widget.commandRequests().size(), 5);
        QCOMPARE(widget.commandRequests()[1].command, Protocol::CMD::SetPidVitesse);
        QCOMPARE(widget.commandRequests()[2].command, Protocol::CMD::SetPidPosition);
        speed->setChecked(false);
        QCOMPARE(widget.commandRequests().size(), 4);
        QCOMPARE(widget.commandRequests().first().command, Protocol::CMD::ModePosition);
        QCOMPARE(widget.commandRequests()[1].command, Protocol::CMD::SetPidPosition);
        position->setChecked(false);
        QVERIFY(speed->isChecked());
        QCOMPARE(widget.commandRequests().first().text(), QString("SET MODE VITESSE"));
        QCOMPARE(widget.commandRequests().size(), 4);
        QCOMPARE(widget.commandRequests()[1].command, Protocol::CMD::SetPidVitesse);
        auto consigne = widget.findChild<ConsigneWidget *>();
        QVERIFY(consigne);
        auto amplitude = consigne->findChild<QDoubleSpinBox *>("amplitude");
        QCOMPARE(amplitude->maximum(), 10.0);
        QCOMPARE(amplitude->suffix(), QString(" rad/s"));
        auto type = speed->findChild<QComboBox *>("typePid");
        auto ki = speed->findChild<QDoubleSpinBox *>("ki");
        auto kd = speed->findChild<QDoubleSpinBox *>("kd");
        QVERIFY(!ki->isEnabled() && !kd->isEnabled());
        type->setCurrentText("PID"); ki->setValue(4); kd->setValue(2);
        QCOMPARE(speed->getCommand(Protocol::CMD::SetPidVitesse).first().values.join(' '), QString("1 4 2"));
        type->setCurrentText("PI");
        QVERIFY(ki->isEnabled() && !kd->isEnabled());
        QCOMPARE(speed->getCommand(Protocol::CMD::SetPidVitesse).first().values.join(' '), QString("1 4 0"));
        type->setCurrentText("P");
        QCOMPARE(speed->getCommand(Protocol::CMD::SetPidVitesse).first().values.join(' '), QString("1 0 0"));
        settings->setChecked(false);
        speed->setChecked(false); position->setChecked(false);
        settings->setChecked(true);
        QVERIFY(speed->isChecked());
        speed->setChecked(false);
        QVERIFY(position->isChecked());
        position->setChecked(false);
        QVERIFY(speed->isChecked());
        EventQueue queue;
        auto measure = std::make_shared<MeasureEvent>();
        measure->time = 2000;
        queue.push(measure);
        widget.processMeasurements(queue.since(0));
        QVERIFY(graph->history().measures.empty());
        QVERIFY(!graph->findChild<QGroupBox *>("centraleInertielle")->isEnabled());
        QCOMPARE(speed->getCommand(Protocol::CMD::SetPidVitesse).first().text(), QString("SET PID VITESSE 1 0 0"));
        QCOMPARE(consigne->getCommand().size(), 1);
        widget.resize(1200, 760); widget.show();
        QTest::qWait(50);
        QVERIFY(!widget.findChild<QObject *>("pilotageProgress"));
        QVERIFY(!widget.grab().isNull());
    }
    void colorCheckBoxInteraction() {
        ColorCheckBox check("Courbe", QColor("#D55E00"));
        AxisColorCheckBox axis("X", check.color());
        QCOMPARE(axis.sizeHint().width(), 20);
        QCOMPARE(axis.indicatorSize(), 20);
        // A different theme size must be applied before the first layout query.
        AxisColorCheckBox largerAxis("Y", check.color());
        largerAxis.setStyleSheet("AxisColorCheckBox { qproperty-indicatorSize: 28; }");
        QCOMPARE(largerAxis.sizeHint().width(), 28);
        QCOMPARE(largerAxis.indicatorSize(), 28);
        QCOMPARE(axis.accessibleName(), QString("X"));
        axis.show();
        const QImage unchecked = axis.grab().toImage();
        QTest::mouseClick(&axis, Qt::LeftButton, Qt::NoModifier, axis.rect().center());
        QVERIFY(axis.isChecked());
        QVERIFY(unchecked != axis.grab().toImage());
        QTest::keyClick(&axis, Qt::Key_Space);
        QVERIFY(!axis.isChecked());
        axis.setColor(QColor("#0072B2"));
        QCOMPARE(axis.color(), QColor("#0072B2"));
        QVERIFY(unchecked != axis.grab().toImage());
    }
    void consigneUnitCsvRoundtrip() {
        QTemporaryDir directory;
        for (const auto &unit : {QString("PWM"), QString("rad/s"), QString::fromUtf8("°")}) {
            GraphWidget graph;
            graph.setConsigneUnit(unit);
            auto measure = std::make_shared<MeasureEvent>();
            measure->target = 1250;
            EventQueue::Batch batch;
            batch.entries = {{1, measure}};
            graph.append(batch);
            const auto path = directory.filePath("original.csv");
            QVERIFY(graph.saveCsv(path));
            VisuPage page;
            QString error;
            QVERIFY2(page.load(path, &error), qPrintable(error));
            auto restored = page.findChild<GraphFileWidget *>();
            QCOMPARE(restored->findChild<QCheckBox *>("Mesures_target")->text(),
                     "Consigne (" + unit + ")");
            const auto copy = directory.filePath("copy.csv");
            QVERIFY(restored->saveCsv(copy));
            QFile original(path), saved(copy);
            QVERIFY(original.open(QIODevice::ReadOnly));
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), original.readAll());
        }
    }
    void graphiqueOriginTime() {
        GraphWidget graph;
        auto measure = std::make_shared<MeasureEvent>();
        measure->time = 1250;
        EventQueue::Batch batch;
        batch.entries = {{1, measure}};
        graph.append(batch);
        auto time = graph.findChild<QLabel *>("graphiqueTemps");
        QCOMPARE(time->text(), QString("Temps : 0.00 s"));
        graph.setOriginTime(1000);
        QCOMPARE(time->text(), QString("Temps : 0.25 s"));
        QCOMPARE(graph.history().measures.front().get(), measure.get());
        QCOMPARE(measure->time, quint32(1250));
        graph.setOriginTime(0);
        QCOMPARE(time->text(), QString("Temps : 1.25 s"));
        graph.setOriginTime(1000);
        graph.reset();
        graph.append(batch);
        QCOMPARE(time->text(), QString("Temps : 0.00 s"));
    }
    void graphiqueDataAndExport() {
        GraphWidget graph;
        auto saveButton = graph.findChild<QPushButton *>("boutonSauvegarder");
        QVERIFY(saveButton && !saveButton->isEnabled());
        QCOMPARE(graph.findChildren<QCheckBox *>().size(), 15);
        auto voltage = graph.findChild<QCheckBox *>("Mesures_voltage");
        QVERIFY(voltage && voltage->isChecked());
        QVERIFY(graph.findChild<QCheckBox *>("Acceleration_acceleration0")->isChecked());
        auto inertial = graph.findChild<QGroupBox *>("centraleInertielle");
        auto axis = graph.findChild<QCheckBox *>("Acceleration_acceleration0");
        QVERIFY(inertial && inertial->isCheckable() && !inertial->isChecked());
        QVERIFY(!axis->isEnabled());
        inertial->setChecked(true);
        axis->setChecked(true);
        inertial->setChecked(false);
        QVERIFY(!axis->isEnabled());
        QVERIFY(axis->isChecked());
        QVERIFY(voltage->isEnabled());
        inertial->setChecked(true);
        QVERIFY(axis->isEnabled());
        QVERIFY(axis->isChecked());
        axis->setChecked(false);
        MeasurementStore store;
        auto m = std::make_shared<MeasureEvent>();
        m->time = 1250; m->voltage = -1200; m->current = -150;
        store.measures.push_back(m);
        auto a = std::make_shared<AccelerometerEvent>(); a->time = 1300; a->acceleration[0] = -42;
        store.accelerometer.push_back(a);
        graph.refresh(store);
        bool motorVoltageDisplayed = false;
        for (auto label : graph.findChildren<QLabel *>())
            if (label->text() == QString::fromUtf8("Tension moteur : -12 V")) motorVoltageDisplayed = true;
        QVERIFY(motorVoltageDisplayed);
        QVERIFY(saveButton->isEnabled());
        QCOMPARE(graph.findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : 0.00 s"));
        QTemporaryDir directory;
        const auto path = directory.filePath("courbes.csv");
        QVERIFY(graph.saveCsv(path));
        QFile csv(path); QVERIFY(csv.open(QIODevice::ReadOnly));
        const auto data = csv.readAll();
        QVERIFY(data.startsWith("# Blum Mesures - " BLUM_VERSION " - "));
        // Signed motor voltage: -1200 with factor 0.01 yields -12 V.
        QVERIFY(data.contains(QLocale().toString(-12.0, 'g', 17).toUtf8()));
        QVERIFY(data.contains(QLocale().toString(1.5, 'g', 17).toUtf8()));
        QVERIFY(data.contains("-42"));
        QVERIFY(!data.contains("Temps IHM"));
        VisuPage restoredPage;
        auto &restored = *restoredPage.findChild<GraphFileWidget *>();
        QString error;
        QVERIFY2(restoredPage.load(path, &error), qPrintable(error));
        QVERIFY(restored.findChild<QGroupBox *>("centraleInertielle")->isEnabled());
        QCOMPARE(restored.findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : 0.00 s"));
        const auto copy = directory.filePath("copy.csv");
        QVERIFY(restored.saveCsv(copy));
        QFile copied(copy); QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), data);
        csv.close();
        graph.resize(1100, 700); graph.show();
        QVERIFY(!graph.grab().isNull());
        auto canvas = graph.findChild<QWidget *>("graphiqueCanvas");
        QVERIFY(canvas);
        QTest::mouseMove(canvas, canvas->rect().center());
        QCOMPARE(graph.findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : 0.00 s"));
        QEvent leave(QEvent::Leave); QApplication::sendEvent(canvas, &leave);
        QCOMPARE(graph.findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : 0.00 s"));
        graph.autoRange(); graph.reset();
        QVERIFY(graph.history().measures.empty());
        QVERIFY(graph.history().accelerometer.empty());
        QVERIFY(!saveButton->isEnabled());
        QCOMPARE(graph.findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : -- s"));
        QVERIFY(graph.saveCsv(path));
        QVERIFY(csv.open(QIODevice::ReadOnly | QIODevice::Text));
        QVERIFY(csv.readAll().startsWith("# Blum Mesures - " BLUM_VERSION " - "));
        VisuPage emptyPage;
        auto &empty = *emptyPage.findChild<GraphFileWidget *>();
        QVERIFY(emptyPage.load(path));
        QVERIFY(!empty.findChild<QPushButton *>("boutonSauvegarder"));
        auto emptyAccelerometer = empty.findChild<QGroupBox *>("centraleInertielle");
        QVERIFY(emptyAccelerometer);
        QVERIFY(!emptyAccelerometer->isChecked());
        QVERIFY(!emptyAccelerometer->isEnabled());
    }
    void graphiqueIncrementalMatchesRebuild() {
        GraphWidget incremental;
        QTemporaryDir dir;
        const auto actual = dir.filePath("incremental.csv");
        const auto expected = dir.filePath("rebuilt.csv");
        auto compareRebuild = [&](int origin = 10) {
            GraphWidget rebuilt;
            rebuilt.setOriginTime(origin);
            rebuilt.refresh(incremental.history());
            incremental.setRecordedAt("test");
            rebuilt.setRecordedAt("test");
            QVERIFY(incremental.saveCsv(actual));
            QVERIFY(rebuilt.saveCsv(expected));
            QFile a(actual), b(expected);
            QVERIFY(a.open(QIODevice::ReadOnly));
            QVERIFY(b.open(QIODevice::ReadOnly));
            QCOMPARE(a.readAll(), b.readAll());
        };
        quint64 sequence = 0;
        auto appendSamples = [&](int first, int count) {
            EventQueue::Batch batch;
            for (int i = first; i < first + count; ++i) {
                auto m = std::make_shared<MeasureEvent>();
                m->time = quint32(i * 10);
                m->voltage = qint16(i % 100);
                m->target = i;
                batch.entries.push_back({++sequence, m});
                if (i % 4 == 0) {
                    auto a = std::make_shared<AccelerometerEvent>();
                    a->time = m->time + 1;
                    a->acceleration[0] = qint16(i % 200);
                    batch.entries.push_back({++sequence, a});
                }
            }
            incremental.append(batch);
        };
        appendSamples(1, 20);
        compareRebuild();
        appendSamples(21, 20);
        compareRebuild();
        // Both bounded histories evict old samples.
        appendSamples(41, 2100);
        compareRebuild();
        incremental.setOriginTime(100);
        compareRebuild(100);
        incremental.reset();
        appendSamples(1, 10);
        compareRebuild();
    }
    void rawValuesAndStrictAlignment() {
        MeasureEvent raw; raw.time = 0xfffffffeu; raw.voltage = -2400; raw.doorAngle = 65535;
        QCOMPARE(raw.rawValues()[0], qint64(0xfffffffeu));
        QCOMPARE(raw.rawValues()[1], qint64(-2400));
        QCOMPARE(raw.rawValues()[5], qint64(65535));
        MeasurementStore store;
        // Construct inertial samples first: host creation order must not affect matching.
        for (quint32 t : {50u, 200u, 250u, 260u}) {
            auto a = std::make_shared<AccelerometerEvent>(); a->time = t; a->acceleration[0] = t;
            store.accelerometer.push_back(a);
        }
        for (quint32 t : {100u, 200u, 300u}) {
            auto m = std::make_shared<MeasureEvent>(); m->time = t; store.measures.push_back(m);
        }
        GraphWidget graph; graph.refresh(store);
        QTemporaryDir dir; const auto path = dir.filePath("aligned.csv");
        QVERIFY(graph.saveCsv(path));
        VisuPage copyPage;
        auto &copy = *copyPage.findChild<GraphFileWidget *>(); QString error;
        QVERIFY2(copyPage.load(path, &error), qPrintable(error));
        const auto second = dir.filePath("copy.csv"); QVERIFY(copy.saveCsv(second));
        QFile a(path), b(second); QVERIFY(a.open(QIODevice::ReadOnly)); QVERIFY(b.open(QIODevice::ReadOnly));
        const auto bytes = a.readAll(); QCOMPARE(bytes, b.readAll());
        const QChar separator = QLocale().decimalPoint() == "," ? ';' : ',';
        int samples = 0;
        for (const auto &line : QString::fromUtf8(bytes).split('\n')) {
            const auto fields = line.split(separator);
            if (fields.size() != 16 || fields[7].isEmpty() || fields[0].startsWith('"')) continue;
            bool ok; const auto value = QLocale().toDouble(fields[7], &ok); QVERIFY(ok);
            const auto time = QLocale().toDouble(fields[0], &ok); QVERIFY(ok);
            QVERIFY(value == 200 || value == 250);
            QCOMPARE(time, value == 200 ? 0.1 : 0.2); ++samples;
        }
        QCOMPARE(samples, 2);
        QCOMPARE(static_cast<const AccelerometerEvent &>(*store.accelerometer.back()).time, quint32(260));
    }
    void errorMessageCapitalizationAndWrapping() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        MainWindow window(nullptr, &serial);
        for (const auto &message : {QString::fromUtf8("  \t erreur <carte>  "),
                                   QString::fromUtf8("  échec de communication. ").repeated(25),
                                   QString(600, 'w'), QString()}) {
            serial.deviceError(message);
            auto dialog = window.findChild<QMessageBox *>("errorDialog");
            QVERIFY(dialog);
            auto expected = message.trimmed();
            if (!expected.isEmpty()) expected.replace(0, 1, expected.left(1).toUpper());
            QCOMPARE(dialog->text(), expected);
            QVERIFY(dialog->informativeText().isEmpty());
            QCOMPARE(dialog->textFormat(), Qt::PlainText);
            QApplication::processEvents();
            if (!expected.isEmpty()) {
                auto label = dialog->findChild<QLabel *>("qt_msgbox_label");
                QVERIFY(label); QVERIFY(label->wordWrap());
                QCOMPARE(label->width(), 390);
                QVERIFY(label->height() >= label->heightForWidth(label->width()));
            }
            QPointer<QMessageBox> guard(dialog);
            dialog->accept();
            QTRY_VERIFY(guard.isNull());
        }
    }
    void errorDialogPresentation() {
        const auto previousStyle = qApp->styleSheet();
        QFile style(":/styles.qss"); QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(style.readAll()));
        QMessageBox shortMessage(QMessageBox::Critical, "Erreur", "Erreur courte", QMessageBox::Ok);
        shortMessage.setTextFormat(Qt::PlainText);
        shortMessage.setInformativeText(shortMessage.text());
        shortMessage.setText(shortMessage.windowTitle());
        shortMessage.show(); QApplication::processEvents();
        QMessageBox longMessage(QMessageBox::Warning, "Erreur", QString("Une description longue de l'erreur. ").repeated(30), QMessageBox::Ok);
        longMessage.setTextFormat(Qt::PlainText);
        longMessage.setInformativeText(longMessage.text());
        longMessage.setText(longMessage.windowTitle());
        longMessage.show(); QApplication::processEvents();
        const auto label = longMessage.findChild<QLabel *>("qt_msgbox_informativelabel");
        QVERIFY(label); QVERIFY(label->wordWrap());
        QCOMPARE(longMessage.textFormat(), Qt::PlainText);
        QCOMPARE(shortMessage.width(), longMessage.width());
        QCOMPARE(label->width(), 390);
        QVERIFY(longMessage.height() > shortMessage.height());
        QVERIFY(label->height() >= label->heightForWidth(label->width()));
        QVERIFY(label->height() > label->fontMetrics().height());
        for (auto box : {&shortMessage, &longMessage}) {
            QCOMPARE(box->text(), box->windowTitle());
        }
        QCOMPARE(longMessage.informativeText(), QString("Une description longue de l'erreur. ").repeated(30));
        QMessageBox pathMessage(QMessageBox::Critical, "Erreur", QString(600, 'W'), QMessageBox::Ok);
        pathMessage.setTextFormat(Qt::PlainText);
        pathMessage.setInformativeText(pathMessage.text());
        pathMessage.setText(pathMessage.windowTitle());
        pathMessage.show(); QApplication::processEvents();
        const auto pathLabel = pathMessage.findChild<QLabel *>("qt_msgbox_informativelabel"); QVERIFY(pathLabel);
        QCOMPARE(pathMessage.informativeText(), QString(600, 'W'));
        QVERIFY(pathLabel->wordWrap());
        QVERIFY(pathLabel->height() > pathLabel->fontMetrics().height());
        QCOMPARE(pathMessage.width(), shortMessage.width());
        qApp->setStyleSheet(previousStyle);
    }
    void measurementCsvVersions() {
        QTemporaryDir dir;
        GraphWidget graph;
        const auto path = dir.filePath("versions.csv");
        QVERIFY(graph.saveCsv(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll(); file.close();
        for (const auto &version : {QByteArray("0.1"), QByteArray("0.1.0"), QByteArray("0.1.9"), QByteArray("999.0.0"), QByteArray("invalid")}) {
            auto bytes = original; bytes.replace(BLUM_VERSION, version);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(file.write(bytes), qint64(bytes.size())); file.close();
            VisuPage reader;
            QString error; QVERIFY(!reader.load(path, &error));
            if (version.startsWith("0.1")) {
                QVERIFY(error.contains(QString::fromUtf8("antérieure")));
                QVERIFY(error.contains(QString::fromUtf8(version)));
                QVERIFY(error.contains(BLUM_VERSION));
            } else QVERIFY(!error.isEmpty());
        }
    }
    void csvDecimalPrecision() {
        QTemporaryDir dir;
        for (const auto &unit : {QString("s"), QString("ms")}) {
            QTemporaryFile config; QVERIFY(config.open());
            config.write((QString("{\"time\":{\"unite\":\"%1\",\"facteur\":%2},")
                .arg(unit, unit == "s" ? "0.001" : "1")
                + "\"courbes\":[{\"source\":\"Mesures\",\"id\":1,\"nom\":\"Tension\",\"facteur\":0.001,\"couleur\":\"#000000\"}]}").toUtf8());
            QVERIFY(config.flush());
            GraphWidget graph(nullptr, config.fileName());
            auto m = std::make_shared<MeasureEvent>(); m->time = 1234; m->voltage = -1236;
            EventQueue::Batch batch; batch.entries = {{1, m}}; graph.append(batch);
            const auto path = dir.filePath(unit + ".csv"); QVERIFY(graph.saveCsv(path));
            QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto row = QString::fromUtf8(file.readAll()).trimmed().split('\n').last();
            const auto locale = QLocale();
            const auto separator = locale.decimalPoint() == "," ? ';' : ',';
            QCOMPARE(row, (unit == "s" ? locale.toString(1.234, 'f', 4) : QString("1234"))
                + separator + locale.toString(-1.24, 'f', 2));
        }
    }
    void measurementCsvValidation() {
        const auto previousLocale = QLocale();
        QTemporaryDir directory;
        for (const auto &locale : {QLocale(QLocale::French), QLocale::c()}) {
            QLocale::setDefault(locale);
            GraphWidget graph;
            MeasurementStore store;
            for (int i = 0; i < 3; ++i) {
                auto m = std::make_shared<MeasureEvent>();
                m->time = 1250 + i * 7; m->current = -123;
                store.measures.push_back(m);
            }
            auto a = std::make_shared<AccelerometerEvent>();
            a->time = 1299; a->acceleration[0] = -42;
            store.accelerometer.push_back(a);
            graph.refresh(store);
            graph.setRecordingMetadata({"Boucle Ouverte - PWM", "CONSIGNE ECHELON 10 1000 0"});
            const auto path = directory.filePath("measurements.csv");
            QVERIFY(graph.saveCsv(path));
            VisuPage restoredPage;
            auto &restored = *restoredPage.findChild<GraphFileWidget *>();
            QString error;
            QVERIFY2(restoredPage.load(path, &error), qPrintable(error));
            QCOMPARE(restored.recordingMetadata(), graph.recordingMetadata());
            QVERIFY(restored.findChild<QGroupBox *>("centraleInertielle")->isChecked());
            QVERIFY(!restored.findChild<QPushButton *>("boutonSauvegarder"));
            const auto copy = directory.filePath("roundtrip.csv");
            QVERIFY(restored.saveCsv(copy));
            QFile original(path), saved(copy);
            QVERIFY(original.open(QIODevice::ReadOnly)); QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto bytes = original.readAll();
            QCOMPARE(saved.readAll(), bytes);
            original.close(); saved.close();
            for (int invalid = 0; invalid < 4; ++invalid) {
                auto corrupt = bytes;
                if (invalid == 0) corrupt.replace("# Blum Mesures", "# Autre format");
                if (invalid == 1) corrupt.replace(BLUM_VERSION, "999.0.0");
                if (invalid == 2) corrupt += "invalid row\n";
                if (invalid == 3) corrupt.replace("-42", "nan");
                QVERIFY(original.open(QIODevice::WriteOnly | QIODevice::Truncate));
                original.write(corrupt); original.close();
                QVERIFY(!restoredPage.load(path, &error));
                QVERIFY(!error.isEmpty());
                QCOMPARE(restored.recordingMetadata(), graph.recordingMetadata());
            }
        }
        QLocale::setDefault(previousLocale);
    }
    void configuredTimeAndMessages() {
        QTemporaryFile configuration;
        QVERIFY(configuration.open());
        configuration.write("{\"time\":{\"nom\":\"Horloge\",\"unite\":\"cs\",\"facteur\":0.01},"
            "\"courbes\":[{\"source\":\"Mesures\",\"id\":\"target\",\"nom\":\"Consigne\",\"unite\":\"deg\","
            "\"facteur\":0.01,\"couleur\":\"#000000\"}]}");
        QVERIFY(configuration.flush());
        GraphWidget graph(nullptr, configuration.fileName());
        auto measure = std::make_shared<MeasureEvent>();
        measure->time = 1250; measure->target = -123; measure->current = -456;
        EventQueue::Batch batch; batch.entries = {{1, measure}};
        graph.append(batch);
        QCOMPARE(graph.history().measures.size(), size_t(1));
        QCOMPARE(graph.findChild<QLabel *>("graphiqueTemps")->text(), QString("Horloge : 12.50 cs"));
        QTemporaryDir directory;
        const auto path = directory.filePath("converted.csv");
        QVERIFY(graph.saveCsv(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto csv = file.readAll();
        QVERIFY(csv.contains("Horloge [cs]"));
        GraphFileWidget restored(nullptr, configuration.fileName());
        QVERIFY(restored.load(QString::fromUtf8(csv).split('\n').mid(1)));
        QCOMPARE(restored.findChild<QLabel *>("graphiqueTemps")->text(), QString("Horloge : 12.50 cs"));
        QCOMPARE(measure->rawValues()[0], qint64(1250));
        QCOMPARE(measure->rawValues()[2], qint64(-456));
        QCOMPARE(measure->rawValues()[6], qint64(-123));
        DebugPage debug;
        debug.processMeasurements(batch);
        auto debugView = debug.findChild<QTextEdit *>("debugMeasures");
        QVERIFY(debugView->toPlainText().contains("1.250s"));
        QVERIFY(debugView->toPlainText().contains("I=-4.56A"));
        QVERIFY(debugView->toPlainText().contains("-1.23"));
    }
    void initTestCase() {
        QVERIFY(AppSettings::initialize());
        QFile theme(":/styles.qss");
        QVERIFY(theme.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(theme.readAll()));
        QVERIFY(!QImage(":/icons/logo.svg").isNull());
        QCoreApplication::setOrganizationName("BlumTests");
        QCoreApplication::setApplicationName("Integration");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
    }
    void debugTextStyles() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        MainWindow router(nullptr, &serial);
        DebugPage page; router.connectPage(&page);
        SharedQueues queues;
        for (char type : {'E', 'N', 'W', 'S', 'I', 'O'}) {
            auto event = std::make_shared<MessageEvent>();
            event->type = type; event->code = 1;
            queues.events.push(event);
        }
        auto request = std::make_shared<RequestEvent>(); request->command = "<tag>&";
        queues.events.push(request);
        auto plain = std::make_shared<TextEvent>(); plain->line = "Texte brut\nsuite " + QString("long ").repeated(50);
        queues.events.push(plain);
        page.processEvents(queues.events.since(0));
        page.processMeasurements(queues.measures.since(0));
        auto view = page.findChild<QTextEdit *>("debugEvents"); QVERIFY(view);
        view->setFixedWidth(200);
        page.show();
        QCoreApplication::processEvents();
        view->document()->documentLayout()->documentSize(); // Force wrapping layout.
        auto block = view->document()->begin();
        QVERIFY(block.text().startsWith("  Erreur : "));
        QVERIFY(!block.text().contains("Erreur :   Erreur"));
        for (const auto &color : {"#DC2626", "#DC2626", "#F75A1B", "#16A34A", "#2563EB", "#2563EB"}) {
            QVERIFY(block.isValid());
            QCOMPARE(block.begin().fragment().charFormat().foreground().color(), QColor(color));
            block = block.next();
        }
        QCOMPARE(block.text(), QString::fromUtf8("▶ <tag>&")); // Plain text, not HTML.
        QVERIFY(block.begin().fragment().charFormat().fontUnderline());
        block = block.next();
        QVERIFY(block.text().startsWith("  Texte brut"));
        QVERIFY(block.text().contains(QChar::LineSeparator));
        const qreal indent = 2 * QFontMetricsF(view->font()).horizontalAdvance(' ');
        QCOMPARE(block.blockFormat().leftMargin(), indent);
        QCOMPARE(block.blockFormat().textIndent(), -indent);
        QVERIFY(block.layout()->lineCount() > 1);
        const auto firstLine = block.layout()->lineAt(0);
        const auto nextLine = block.layout()->lineAt(1);
        QVERIFY(qAbs(nextLine.x() - firstLine.x() - indent) < 0.1);
        const auto format = block.begin().fragment().charFormat();
        QVERIFY(!format.fontUnderline());
        QVERIFY(!format.hasProperty(QTextFormat::ForegroundBrush));

        auto measure = std::make_shared<MeasureEvent>();
        measure->time = 1234; measure->current = -456; measure->target = -123;
        auto acceleration = std::make_shared<AccelerometerEvent>();
        acceleration->time = 2345; acceleration->velocity = {-123, 456, -789};
        queues.measures.push(measure);
        queues.measures.push(acceleration);
        page.processMeasurements(queues.measures.since(0));
        const auto text = page.findChild<QTextEdit *>("debugMeasures")->toPlainText();
        QVERIFY(text.contains("1.234s"));
        QVERIFY(text.contains("I=-4.56A"));
        QVERIFY(text.contains("2.345s"));
        QVERIFY(text.contains("-7.89"));
        QCOMPARE(measure->rawValues()[2], qint64(-456));
        QCOMPARE(acceleration->rawValues()[6], qint64(-789));
        QVERIFY(!measure->content().contains("Cons=")); // Les événements ne connaissent pas le document.
    }
    void stateGraphConfiguration() {
        QTemporaryFile definitions;
        QVERIFY(definitions.open());
        const QByteArray json = R"({"default_machine":42,"machines":[
            {"code":42,"etats":[{"code":7,"nom":"MODE"}]},
            {"code":43,"default_state":99,"display_when":{"machine":42,"state":7},
             "etats":[{"code":99,"nom":"TERMINE"},{"code":100,"nom":"EN COURS"}]}
        ]})";
        QCOMPARE(definitions.write(json), qint64(json.size()));
        QVERIFY(definitions.flush());
        StateGraphWidget graph(nullptr, definitions.fileName());
        QCOMPARE(graph.currentMachineCode(), 42);
        QCOMPARE(graph.stateDescription(43), QString("99 : TERMINE"));
        graph.setState(42, 7);
        QCOMPARE(graph.currentMachineCode(), 43);
        graph.setState(43, 100);
        QTRY_COMPARE(graph.activeStateName(), QString("EN COURS"));
        graph.setState(42, 8);
        QTRY_COMPARE(graph.currentMachineCode(), 42);
        graph.reset();
        QCOMPARE(graph.currentMachineCode(), 42);
        QCOMPARE(graph.stateDescription(42), QString("inconnu"));
        QCOMPARE(graph.stateDescription(43), QString("99 : TERMINE"));
    }
    void stateGraphQueuesRapidStates() {
        StateGraphWidget graph;
        graph.setState(1);
        graph.setState(3);
        graph.setState(3);
        graph.setState(4);
        QCOMPARE(graph.activeStateName(), QString("REPOS"));
        QTest::qSleep(5);
        QCoreApplication::processEvents();
        QCOMPARE(graph.activeStateName(), QString("REPOS"));
        QTest::qSleep(50);
        QCoreApplication::processEvents();
        QCOMPARE(graph.activeStateName(), QString("OUVERTURE"));
        QTRY_COMPARE(graph.activeStateName(), QString("FERMETURE"));
        graph.setState(9);
        graph.reset();
        QTest::qWait(50);
        QCOMPARE(graph.activeStateName(), QString("inconnu"));
    }
    void stateGraphRoutesStatesByThreshold() {
        StateGraphWidget graph;
        graph.setState(19);
        QVERIFY(graph.stateDescription(250).startsWith("19 : "));
        graph.setState(20);
        QTRY_VERIFY(graph.stateDescription(251).startsWith("20 : "));
        QVERIFY(graph.stateDescription(250).startsWith("19 : "));

        StateGraphWidget custom(nullptr, ":/machines.json", 30);
        custom.setState(29);
        QVERIFY(custom.stateDescription(250).startsWith("29 : "));
        custom.setState(30);
        QTRY_VERIFY(custom.stateDescription(251).startsWith("30 : "));
    }

    void stateGraphCommandKeys() {
        StateGraphWidget graph;
        graph.resize(400, 600);
        const auto production = AppSettings::instance().states().value("machines").toArray().first().toObject();
        const auto rest = production.value("etats").toArray().at(1).toObject();
        QCOMPARE(rest.value("cmd").toString(), QString("DO STOP"));
        const QPoint center(qRound(rest.value("x").toDouble() * graph.contentsRect().width()),
                            qRound(rest.value("y").toDouble() * graph.contentsRect().height()));
        QSignalSpy commands(&graph, &StateGraphWidget::sendRawCommand);
        QTest::mouseClick(&graph, Qt::LeftButton, Qt::NoModifier, center);
        QVERIFY(commands.isEmpty());
        graph.setEnabled(true);
        QTest::mouseClick(&graph, Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands[0][0].toString(), QString("DO STOP"));
    }
    void stateGraphSwitchingAndCommands() {
        StateGraphWidget graph;
        graph.ensurePolished();
        QCOMPARE(graph.property("stateBorderColor").value<QColor>(), QColor("#64748B"));
        QCOMPARE(graph.property("activeBackgroundColor").value<QColor>(), QColor("#FFEDD5"));
        QCOMPARE(graph.property("hoverBorderColor").value<QColor>(), QColor("#F75A1B"));
        graph.resize(350, 600);
        QCOMPARE(graph.currentMachineCode(), 250);
        QCOMPARE(graph.stateDescription(251), QString("28 : FIN"));
        graph.setState(251, 23);
        QCOMPARE(graph.currentMachineCode(), 250);
        graph.setState(250, 6);
        QTRY_COMPARE(graph.currentMachineCode(), 251);
        QCOMPARE(graph.activeStateName(), QString("BUTEE_BASSE"));
        graph.setState(250, 1);
        QTRY_COMPARE(graph.currentMachineCode(), 250);
        QCOMPARE(graph.activeStateName(), QString("REPOS"));
        QSignalSpy commands(&graph, &StateGraphWidget::sendRawCommand);
        QFile states(":/machines.json");
        QVERIFY(states.open(QIODevice::ReadOnly));
        const auto production = QJsonDocument::fromJson(states.readAll()).object().value("machines").toArray().first().toObject();
        const auto restState = production.value("etats").toArray().at(1).toObject();
        auto restCenter = [&] {
            return QPoint(qRound(restState.value("x").toDouble() * graph.contentsRect().width()),
                          qRound(restState.value("y").toDouble() * graph.contentsRect().height()));
        };
        const QPoint rest = restCenter();
        QTest::mouseClick(&graph, Qt::LeftButton, Qt::NoModifier, rest);
        QVERIFY(commands.isEmpty());
        graph.setEnabled(true);
        QTest::mouseClick(&graph, Qt::LeftButton, Qt::NoModifier, rest);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands[0][0].toString(), QString("DO STOP"));
        graph.resize(350, 950);
        const QPoint tallRest = restCenter();
        QTest::mouseClick(&graph, Qt::LeftButton, Qt::NoModifier, tallRest);
        QCOMPARE(commands.size(), 2);
        graph.setState(250, 9);
        QCOMPARE(commands.size(), 2);
        graph.show();
        QVERIFY(!graph.grab().isNull());
        graph.reset();
        QCOMPARE(graph.currentMachineCode(), 250);
        QCOMPARE(graph.activeStateName(), QString("inconnu"));
        QCOMPARE(graph.stateDescription(251), QString("28 : FIN"));
    }
    void deviceNotifications() {
        MainWindow window;
        window.show();
        auto serial = window.findChild<SerialController *>(); QVERIFY(serial);
        auto indicator = window.findChild<QLabel *>("statusSecurite"); QVERIFY(indicator);
        serial->stateReceived(9);
        QVERIFY(indicator->property("state").toString() != "error");
        QVERIFY(!window.findChild<QMessageBox *>("errorDialog"));
        serial->deviceWarning("Attention carte");
        QCOMPARE(indicator->property("state").toString(), QString("warning"));
        QVERIFY(!window.findChild<QMessageBox *>("errorDialog"));
        serial->deviceError("  \t Erreur carte test");
        auto dialog = window.findChild<QMessageBox *>("errorDialog"); QVERIFY(dialog);
        QCOMPARE(dialog->text(), QString("Erreur carte test"));
        QCOMPARE(dialog->windowTitle(), QString("Erreur carte"));
        QVERIFY(dialog->informativeText().isEmpty());
        QCOMPARE(dialog->icon(), QMessageBox::Critical);
        QCOMPARE(indicator->property("state").toString(), QString("error"));
        serial->stateReceived(1);
        QCOMPARE(indicator->property("state").toString(), QString("error"));
        serial->stateReceived(3);
        QCOMPARE(indicator->property("state").toString(), QString("ok"));
        QVERIFY(dialog->isVisible()); // The error still requires acknowledgement.
        QPointer<QMessageBox> guard(dialog);
        dialog->button(QMessageBox::Ok)->click();
        QTRY_VERIFY(guard.isNull());
        for (int state : {Protocol::Etat::PROD_INIT, Protocol::Etat::PROD_OUVERTURE,
                          Protocol::Etat::PROD_FERMETURE, Protocol::Etat::PROD_PILOTAGE,
                          Protocol::Etat::PROD_CALIBRATION}) {
            serial->deviceWarning("Attention carte");
            QCOMPARE(indicator->property("state").toString(), QString("warning"));
            serial->fault("Erreur carte");
            QCOMPARE(indicator->property("state").toString(), QString("error"));
            serial->stateReceived(state);
            QCOMPARE(indicator->property("state").toString(), QString("ok"));
            auto status = window.findChild<StatusWidget *>("statusWidget"); QVERIFY(status);
            QVERIFY(status->lastError().isEmpty());
            QVERIFY(status->lastWarning().isEmpty());
            auto message = window.findChild<QLabel *>("statusSecuriteMessage"); QVERIFY(message);
            QVERIFY(message->text().isEmpty());
        }
    }
    void serialConnectionLostNotification() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        MainWindow window(nullptr, &serial);
        serial.fault("Erreur d'ouverture");
        QVERIFY(!window.findChild<QMessageBox *>("errorDialog"));
        serial.connectionLost("Connexion interrompue sur COM3");
        auto dialog = window.findChild<QMessageBox *>("errorDialog"); QVERIFY(dialog);
        QCOMPARE(dialog->icon(), QMessageBox::Critical);
        QCOMPARE(dialog->windowTitle(), QString::fromUtf8("Connexion série perdue"));
        auto displayedText = dialog->text(); displayedText.remove('\n');
        QCOMPARE(displayedText, QString("Connexion interrompue sur COM3"));
        QVERIFY(dialog->isVisible());
        QCOMPARE(window.findChild<QLabel *>("statusSecurite")->property("state").toString(), QString("error"));
        dialog->accept();
    }
    void frenchStandardButtons() {
        QTranslator translator;
        QVERIFY(translator.load(":/qtbase_fr.qm"));
        QVERIFY(qApp->installTranslator(&translator));
        QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        auto saveText = buttons.button(QDialogButtonBox::Save)->text();
        auto cancelText = buttons.button(QDialogButtonBox::Cancel)->text();
        qApp->removeTranslator(&translator);
        QCOMPARE(saveText.remove('&'), QString("Enregistrer"));
        QCOMPARE(cancelText.remove('&'), QString("Annuler"));
    }
    void defaultWindowAndDebugToggle() {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();
        auto graph = window.findChild<StateGraphWidget *>(); QVERIFY(graph);
        auto acquire = window.findChild<QPushButton *>("boutonRunStop"); QVERIFY(acquire);
        auto calibrate = window.findChild<QPushButton *>("boutonCalibration"); QVERIFY(calibrate);
        const auto acquireLeft = acquire->mapTo(&window, QPoint()).x();
        const auto calibrateRight = calibrate->mapTo(&window, QPoint(calibrate->width(), 0)).x();
        QCOMPARE(graph->mapTo(&window, QPoint()).x(), acquireLeft);
        QCOMPARE(graph->width(), calibrateRight - acquireLeft);
        auto production = window.findChild<QLabel *>("productionState"); QVERIFY(production);
        auto calibration = window.findChild<QLabel *>("calibrationState"); QVERIFY(calibration);
        QVERIFY(calibration->text().contains("28 : FIN"));
        QVERIFY(production->mapTo(&window, QPoint()).y() >= graph->mapTo(&window, QPoint(0, graph->height())).y());
        QCOMPARE(production->mapTo(&window, QPoint()).y(), calibration->mapTo(&window, QPoint()).y());
        auto debug = window.findChild<DebugPage *>(); QVERIFY(debug);
        QVERIFY(!debug->isVisible());
        auto tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        auto footer = window.findChild<QWidget *>("footer"); QVERIFY(footer);
        QCOMPARE(footer->parentWidget(), tabs->parentWidget());
        QCOMPARE(footer->geometry().left(), tabs->geometry().left());
        QCOMPARE(footer->width(), tabs->width());
        window.resize(1400, 850);
        QCoreApplication::processEvents();
        QCOMPARE(footer->geometry().left(), tabs->geometry().left());
        QCOMPARE(footer->width(), tabs->width());
        auto logo = window.findChild<QPushButton *>("logo"); QVERIFY(logo);
        for (int i = 0; i < 3; ++i) logo->click();
        QCOMPARE(tabs->count(), 3); QCOMPARE(tabs->tabText(2), QString("Debug"));
        tabs->setCurrentWidget(debug);
        QVERIFY(debug->isVisible());
        for (int i = 0; i < 3; ++i) logo->click();
        QCOMPARE(tabs->count(), 2);
    }
    void tabsRemainVisibleWhenShrinking() {
        MainWindow window;
        window.show();
        auto tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs);
        auto logo = window.findChild<QPushButton *>("logo"); QVERIFY(logo);
        for (bool debugVisible : {false, true}) {
            if (debugVisible) {
                for (int i = 0; i < 3; ++i) logo->click();
            }
            window.resize(1, 600);
            QCoreApplication::processEvents();
            auto bar = tabs->tabBar();
            QCOMPARE(tabs->count(), debugVisible ? 3 : 2);
            for (int i = 0; i < bar->count(); ++i) {
                QVERIFY(bar->rect().contains(bar->tabRect(i)));
                QVERIFY(bar->tabRect(i).width() >= bar->fontMetrics().horizontalAdvance(bar->tabText(i)));
            }
            const auto left = tabs->cornerWidget(Qt::TopLeftCorner);
            const auto right = tabs->cornerWidget(Qt::TopRightCorner);
            QVERIFY(left->geometry().right() < bar->geometry().left());
            QVERIFY(bar->geometry().right() < right->geometry().left());
        }
    }
    void invalidPortAndImmediateShutdown() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        serial.automatic = false; serial.manualPort = "SoftPorteBlum_INVALID_PORT";
        QSignalSpy fault(&serial, &SerialController::fault);
        QSignalSpy lost(&serial, &SerialController::connectionLost);
        serial.connectPort(); QVERIFY(serial.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
        QVERIFY(!serial.connected()); QVERIFY(!fault.isEmpty());
        QCOMPARE(lost.size(), 0);
        // Détruire le contrôleur alors que le démarrage est encore en file.
        auto second = std::make_unique<SerialController>(nullptr, settings_.filePath("config.ini"));
        second->automatic = false; second->manualPort = "SoftPorteBlum_INVALID_PORT";
        second->connectPort(); second.reset();
    }
    void systemStateNotifications() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        QSignalSpy changes(&serial, &SerialController::systemStateChanged);
        serial.automatic = false;
        serial.manualPort = "SoftPorteBlum_STATE_TEST";
        serial.baud = 57600;
        serial.saveSettings();
        QCOMPARE(changes.size(), 1);
        const auto snapshot = qvariant_cast<SystemState>(changes.first().first());
        QCOMPARE(snapshot.port, serial.manualPort);
        QCOMPARE(snapshot.baud, 57600);
        QVERIFY(!snapshot.connected && !snapshot.initialized);
        QVERIFY(!snapshot.measuring && !snapshot.accelerometer);
        serial.saveSettings();
        QCOMPARE(changes.size(), 1); // Unchanged scans/settings do not notify widgets.
        serial.setAcquisition(true);
        serial.setAccelerometer(true);
        QCOMPARE(changes.size(), 1); // Requests without a connection cannot claim success.
        serial.baud = 115200;
        serial.saveSettings();
        QCOMPARE(changes.size(), 2);
        QCOMPARE(serial.systemState().baud, 115200);
        QCOMPARE(snapshot.baud, 57600); // Signal payloads remain independent snapshots.
    }
    void startupReset() {
#ifndef Q_OS_UNIX
        QSKIP("Simulation par pseudo-terminal disponible uniquement sous Unix.");
#else
        for (bool announceStartup : {false, true}) {
            struct Peer {
                int fd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
                ~Peer() { if (fd >= 0) ::close(fd); }
            } peer;
            QVERIFY(peer.fd >= 0); QVERIFY(::grantpt(peer.fd) == 0); QVERIFY(::unlockpt(peer.fd) == 0);
            SerialController serial(nullptr, settings_.filePath("config.ini")); serial.automatic = false;
            serial.manualPort = QString::fromLocal8Bit(::ptsname(peer.fd));
            QSignalSpy fault(&serial, &SerialController::fault);
            serial.connectPort(); QTRY_VERIFY_WITH_TIMEOUT(serial.connected(), 3000);
            if (announceStartup) {
                QByteArray frame(7, '\0'); frame[0] = '\1'; frame[1] = 'I'; frame[2] = 70;
                QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
            }
            QByteArray sent;
            auto readPeer = [&] {
                char buffer[1024]; const auto count = ::read(peer.fd, buffer, sizeof buffer);
                if (count > 0) sent.append(buffer, count);
                return sent.contains("DO RESET\n");
            };
            if (announceStartup) {
                QTest::qWait(2300); QVERIFY(!readPeer());
            } else {
                QTRY_VERIFY_WITH_TIMEOUT(readPeer(), 3000);
                QTest::qWait(2300); readPeer();
                QCOMPARE(sent, QByteArray("DO RESET\n"));
                const auto events = serial.queues()->events.since(0);
                QCOMPARE(events.entries.size(), 1);
                QCOMPARE(events.entries[0].event->raw, QByteArray("DO RESET\n"));
                // Un début d'initialisation ne suffit pas : le code 71 reste requis.
                QByteArray frame(7, '\0'); frame[0] = '\1'; frame[1] = 'I'; frame[2] = 70;
                QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
                serial.automatic = true;
                QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 4000);
                QVERIFY(!serial.connected()); QCOMPARE(fault.size(), 1);
                QVERIFY(fault[0][0].toString().contains("DO RESET"));
                serial.scan(); QVERIFY(!serial.busy());
            }
            QVERIFY(!serial.canSendCommands());
            serial.disconnectPort(); QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
            readPeer();
            QCOMPARE(sent, announceStartup ? QByteArray() : QByteArray("DO RESET\n"));
        }
#endif
    }
    void pageForwardsWithoutAuthorization() {
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        MainWindow window(nullptr, &serial);
        ReceivingPage page; window.connectPage(&page);
        QSignalSpy sent(&page, &Page::sendCommand);
        page.sendCommand(Protocol::CMD::SetPidVitesse, {"1", "2", "3"}, true);
        QCOMPARE(sent.size(), 1); QCOMPARE(page.replies, 1); QVERIFY(!page.accepted);
        QCOMPARE(page.reply, Protocol::CMD::SetPidVitesse);
        const int previous = page.states;
        SystemState state; state.calibration = true;
        serial.systemStateChanged(state);
        QCOMPARE(page.states, previous + 1);
        QVERIFY(page.systemState().calibration);
    }
    void eventRoutingAndContinuousDebug() {
        MainWindow window;
        auto serial = window.findChild<SerialController *>(); QVERIFY(serial);
        auto tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs);
        auto measures = window.findChild<MeasurePage *>();
        auto pilotage = window.findChild<PilotagePage *>();
        auto debug = window.findChild<DebugPage *>(); QVERIFY(measures && pilotage && debug);
        auto states = measures->findChild<StateGraphWidget *>(); QVERIFY(states);
        auto debugEvents = debug->findChild<QTextEdit *>("debugEvents"); QVERIFY(debugEvents);
        auto debugMeasures = debug->findChild<QTextEdit *>("debugMeasures"); QVERIFY(debugMeasures);
        auto graph = pilotage->findChild<GraphWidget *>(); QVERIFY(graph);
        tabs->setCurrentWidget(pilotage);
        auto state = std::make_shared<StateEvent>(); state->value = 4;
        serial->queues()->events.push(state);
        auto text = std::make_shared<TextEvent>(); text->line = "history-marker";
        serial->queues()->events.push(text);
        auto m = std::make_shared<MeasureEvent>(); m->time = 100;
        serial->queues()->measures.push(m);
        QTRY_COMPARE(states->activeStateName(), QString("FERMETURE"));
        QTRY_COMPARE(graph->history().measures.size(), size_t(1));
        QTRY_COMPARE(debugEvents->toPlainText().count("history-marker"), 1);
        QTRY_VERIFY(!debugMeasures->toPlainText().isEmpty());
        auto logo = window.findChild<QPushButton *>("logo"); QVERIFY(logo);
        for (int i = 0; i < 3; ++i) logo->click();
        tabs->setCurrentWidget(debug);
        QCOMPARE(debugEvents->toPlainText().count("history-marker"), 1);
        QVERIFY(!debugMeasures->toPlainText().isEmpty());
        auto second = std::make_shared<MeasureEvent>(); second->time = 200;
        serial->queues()->measures.push(second);
        auto fresh = std::make_shared<TextEvent>(); fresh->line = "live-marker";
        serial->queues()->events.push(fresh);
        QTRY_VERIFY(debugEvents->toPlainText().contains("live-marker"));
        QCOMPARE(graph->history().measures.size(), size_t(1));
        QCOMPARE(debugEvents->toPlainText().count("history-marker"), 1);
        tabs->setCurrentWidget(measures);
        auto hidden = std::make_shared<TextEvent>(); hidden->line = "hidden-marker";
        serial->queues()->events.push(hidden);
        QTRY_COMPARE(debugEvents->toPlainText().count("hidden-marker"), 1);
        tabs->setCurrentWidget(debug);
        QCOMPARE(debugEvents->toPlainText().count("hidden-marker"), 1);
        QCOMPARE(debugEvents->toPlainText().count("history-marker"), 1);
    }
    void pageResetAndCalibrationState() {
        MeasurePage page;
        SystemState state; state.connected = state.initialized = true;
        page.setSystemState(state);
        auto button = page.findChild<CalibrationButton *>(); QVERIFY(button); QVERIFY(button->isEnabled());
        auto graph = page.findChild<StateGraphWidget *>(); QVERIFY(graph);
        auto event = std::make_shared<StateEvent>(); event->value = 3;
        EventQueue::Batch batch; batch.entries = {{1, event}}; batch.cursor = 1;
        page.processEvents(batch); QCOMPARE(graph->activeStateName(), QString("OUVERTURE"));
        state.calibration = true; page.setSystemState(state); QVERIFY(!button->isEnabled());
        state.calibration = false; state.initialized = false;
        page.setSystemState(state); QVERIFY(graph->activeStateName() != "OUVERTURE");
        QVERIFY(!page.systemState().initialized);
        // No new transition: a repeated uninitialized snapshot leaves the view unchanged.
        page.processEvents(batch); QCOMPARE(graph->activeStateName(), QString("OUVERTURE"));
        page.setSystemState(state); QCOMPARE(graph->activeStateName(), QString("OUVERTURE"));
    }
    void periodicMeasurementDispatch() {
        MainWindow window;
        auto serial = window.findChild<SerialController *>(); QVERIFY(serial);
        auto page = window.findChild<MeasurePage *>(); QVERIFY(page);
        auto graph = page->findChild<GraphWidget *>(); QVERIFY(graph);
        auto measure = std::make_shared<MeasureEvent>(); measure->time = 100;
        auto acceleration = std::make_shared<AccelerometerEvent>(); acceleration->time = 150;
        serial->queues()->measures.push(measure);
        serial->queues()->measures.push(acceleration);
        QTRY_COMPARE(graph->history().measures.size(), size_t(1));
        QTRY_COMPARE(graph->history().accelerometer.size(), size_t(1));
        QCOMPARE(graph->history().measures.front().get(), measure.get());
        QTest::qWait(150);
        QCOMPARE(graph->history().measures.size(), size_t(1));
        QCOMPARE(graph->history().accelerometer.size(), size_t(1));
    }
    void activeTabMeasurementRouting() {
        MainWindow window;
        auto serial = window.findChild<SerialController *>(); QVERIFY(serial);
        auto tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs);
        auto measures = window.findChild<MeasurePage *>(); QVERIFY(measures);
        auto pilotage = window.findChild<PilotagePage *>(); QVERIFY(pilotage);
        auto debug = window.findChild<DebugPage *>(); QVERIFY(debug);
        auto measureGraph = measures->findChild<GraphWidget *>(); QVERIFY(measureGraph);
        auto pilotageGraph = pilotage->findChild<GraphWidget *>(); QVERIFY(pilotageGraph);
        const auto push = [&](quint32 time) {
            auto event = std::make_shared<MeasureEvent>(); event->time = time;
            serial->queues()->measures.push(event);
        };
        push(100);
        QTRY_COMPARE(measureGraph->history().measures.size(), size_t(1));
        QVERIFY(pilotageGraph->history().measures.empty());
        push(200); // Already received before the switch, but not yet polled.
        tabs->setCurrentWidget(pilotage);
        QCOMPARE(measureGraph->history().measures.size(), size_t(2));
        QVERIFY(pilotageGraph->history().measures.empty());
        push(300);
        QTRY_COMPARE(pilotageGraph->history().measures.size(), size_t(1));
        QCOMPARE(measureGraph->history().measures.size(), size_t(2));
        auto acceleration = std::make_shared<AccelerometerEvent>(); acceleration->time = 350;
        serial->queues()->measures.push(acceleration);
        QTRY_COMPARE(pilotageGraph->history().accelerometer.size(), size_t(1));
        QVERIFY(measureGraph->history().accelerometer.empty());
        tabs->addTab(debug, "Debug");
        tabs->setCurrentWidget(debug);
        push(400);
        QTest::qWait(100);
        QCOMPARE(pilotageGraph->history().measures.size(), size_t(1));
        QCOMPARE(measureGraph->history().measures.size(), size_t(2));
        tabs->setCurrentWidget(measures);
        push(500);
        QTRY_COMPARE(measureGraph->history().measures.size(), size_t(3));
        QCOMPARE(pilotageGraph->history().measures.size(), size_t(1));
        QCOMPARE(static_cast<const MeasureEvent &>(*measureGraph->history().measures.back()).time, quint32(500));
        QCOMPARE(serial->queues()->measures.since(0).entries.size(), 6); // Debug retains the complete raw stream.
    }
    void acquisitionBuffers() {
#ifndef Q_OS_UNIX
        QSKIP("Simulation par pseudo-terminal disponible uniquement sous Unix.");
#else
        struct Peer {
            int fd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
            ~Peer() { if (fd >= 0) ::close(fd); }
        } peer;
        QVERIFY(peer.fd >= 0); QVERIFY(::grantpt(peer.fd) == 0); QVERIFY(::unlockpt(peer.fd) == 0);
        SerialController serial(nullptr, settings_.filePath("config.ini")); serial.automatic = false;
        serial.manualPort = QString::fromLocal8Bit(::ptsname(peer.fd));
        serial.connectPort(); QTRY_VERIFY_WITH_TIMEOUT(serial.connected(), 3000);
        const auto report = [&](char type, quint8 code, qint32 value) {
            QByteArray frame(7, '\0'); frame[0] = '\1'; frame[1] = type; frame[2] = char(code);
            qToLittleEndian<qint32>(value, frame.data() + 3);
            return ::write(peer.fd, frame.constData(), frame.size());
        };
        QCOMPARE(report('I', 71, 0), ssize_t(7));
        QTRY_VERIFY(serial.canSendCommands());
        MainWindow router(nullptr, &serial);
        MeasurePage measures; router.connectPage(&measures);
        PilotagePage pilotage; router.connectPage(&pilotage);
        auto measureGraph = measures.findChild<GraphWidget *>(); QVERIFY(measureGraph);
        auto pilotageGraph = pilotage.findChild<GraphWidget *>(); QVERIFY(pilotageGraph);
        // Historical samples must not be replayed into the new acquisition.
        for (quint32 time : {100u, 200u, 300u}) {
            auto event = std::make_shared<MeasureEvent>(); event->time = time; event->motorAngle = qint32(time);
            serial.queues()->measures.push(event);
        }
        measures.processMeasurements(serial.queues()->measures.since(0));
        QCOMPARE(measureGraph->history().measures.size(), size_t(3));
        auto acquire = measures.findChild<QPushButton *>("boutonRunStop"); QVERIFY(acquire);
        acquire->click();
        QVERIFY(measureGraph->history().measures.empty());
        measures.processMeasurements(serial.queues()->measures.since(3));
        QVERIFY(measureGraph->history().measures.empty());
        QByteArray sent;
        const auto readPeer = [&] {
            char buffer[4096]; const auto count = ::read(peer.fd, buffer, sizeof buffer);
            if (count > 0) sent.append(buffer, count);
            return sent;
        };
        QTRY_COMPARE(readPeer(), QByteArray("SET MESURES ON\n"));
        QCOMPARE(report('O', 151, 20), ssize_t(7));
        QTRY_VERIFY(serial.systemState().measuring);
        for (quint32 time : {400u, 500u, 600u}) {
            auto event = std::make_shared<MeasureEvent>(); event->time = time; event->motorAngle = qint32(time);
            serial.queues()->measures.push(event);
        }
        auto acceleration = std::make_shared<AccelerometerEvent>(); acceleration->time = 650;
        serial.queues()->measures.push(acceleration);
        measures.processMeasurements(serial.queues()->measures.since(3));
        QCOMPARE(measureGraph->history().measures.size(), size_t(3));
        pilotageGraph->append(serial.queues()->measures.since(0));
        const auto oldSize = pilotageGraph->history().measures.size();
        const auto commands = pilotage.commandRequests();
        auto run = pilotage.findChild<QPushButton *>("boutonRunStop"); QVERIFY(run);
        sent.clear(); run->click();
        for (const auto &command : commands) {
            QTRY_COMPARE(readPeer(), command.text().toUtf8() + '\n');
            QCOMPARE(pilotageGraph->history().measures.size(), oldSize); // Kept until all replies succeed.
            sent.clear();
            QCOMPARE(report('O', command.command == Protocol::CMD::MesuresOn ? 151 : 0, 20), ssize_t(7));
        }
        QTRY_COMPARE(pilotageGraph->history().measures.size(), size_t(1));
        const auto baseline = std::dynamic_pointer_cast<const MeasureEvent>(pilotageGraph->history().measures.front());
        QVERIFY(baseline); QCOMPARE(baseline->time, quint32(600)); QCOMPARE(baseline->motorAngle, 600);
        QVERIFY(pilotageGraph->history().accelerometer.empty());
        auto next = std::make_shared<MeasureEvent>(); next->time = 700;
        serial.queues()->measures.push(next);
        pilotage.processMeasurements(serial.queues()->measures.since(7));
        QCOMPARE(pilotageGraph->history().measures.size(), size_t(2));
        QCOMPARE(static_cast<const MeasureEvent &>(*pilotageGraph->history().measures.back()).time, quint32(700));
        QCOMPARE(next->time, quint32(700)); // Shared raw samples retain their board timestamps.
        QCOMPARE(pilotageGraph->findChild<QLabel *>("graphiqueTemps")->text(), QString("Temps : 0.10 s"));
        pilotage.stopPilotage();
        serial.disconnectPort(); QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
#endif
    }
    void tabSwitchStops() {
#ifndef Q_OS_UNIX
        QSKIP("Simulation par pseudo-terminal disponible uniquement sous Unix.");
#else
        struct Peer {
            int fd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
            ~Peer() { if (fd >= 0) ::close(fd); }
        } peer;
        QVERIFY(peer.fd >= 0); QVERIFY(::grantpt(peer.fd) == 0); QVERIFY(::unlockpt(peer.fd) == 0);
        SerialController serial(nullptr, settings_.filePath("config.ini"));
        serial.automatic = false; serial.manualPort = QString::fromLocal8Bit(::ptsname(peer.fd));
        serial.connectPort(); QTRY_VERIFY_WITH_TIMEOUT(serial.connected(), 3000);
        auto report = [&](char type, quint8 code, qint32 value) {
            QByteArray frame(7, '\0'); frame[0] = '\1'; frame[1] = type; frame[2] = char(code);
            qToLittleEndian<qint32>(value, frame.data() + 3);
            return ::write(peer.fd, frame.constData(), frame.size());
        };
        QCOMPARE(report('I', 71, 0), ssize_t(7));
        QTRY_VERIFY(serial.canSendCommands());
        MainWindow window(nullptr, &serial);
        auto tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs);
        auto measures = window.findChild<MeasurePage *>(); QVERIFY(measures);
        auto pilotage = window.findChild<PilotagePage *>(); QVERIFY(pilotage);
        auto debug = window.findChild<DebugPage *>(); QVERIFY(debug);
        tabs->addTab(debug, "Debug");
        QByteArray sent;
        const auto readPeer = [&] {
            char buffer[4096];
            const auto count = ::read(peer.fd, buffer, sizeof buffer);
            if (count > 0) sent.append(buffer, count);
            return sent;
        };
        QCOMPARE(report('O', 151, 20), ssize_t(7));
        QTRY_VERIFY(serial.systemState().measuring);
        tabs->setCurrentWidget(pilotage);
        QTRY_COMPARE(readPeer(), QByteArray("SET MESURES OFF\n"));
        QVERIFY(serial.systemState().measuring); // Remains active until confirmed by the board.
        QCOMPARE(report('O', 150, 0), ssize_t(7));
        QTRY_VERIFY(!serial.systemState().measuring);
        sent.clear();
        tabs->setCurrentWidget(measures);
        QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(measures));
        QVERIFY(tabs->tabBar()->isEnabled());
        QTest::qWait(30);
        QVERIFY(readPeer().isEmpty()); // No acquisition to stop.
        tabs->setCurrentWidget(pilotage);
        QCOMPARE(report('O', 151, 20), ssize_t(7));
        QTRY_VERIFY(serial.systemState().measuring);
        auto run = pilotage->findChild<QPushButton *>("boutonRunStop"); QVERIFY(run);
        run->click();
        for (const auto &request : pilotage->commandRequests()) {
            QTRY_COMPARE(readPeer(), request.text().toUtf8() + '\n');
            sent.clear();
            QCOMPARE(report('O', request.command == Protocol::CMD::MesuresOn ? 151 : 0, 20), ssize_t(7));
        }
        sent.clear();
        tabs->setCurrentWidget(debug);
        QTest::qWait(30);
        QVERIFY(readPeer().isEmpty());
        tabs->setCurrentWidget(measures);
        QCOMPARE(tabs->currentWidget(), static_cast<QWidget *>(measures));
        QVERIFY(!run->isChecked());
        QTRY_COMPARE(readPeer(), QByteArray("DO STOP\nSET MESURES OFF\n"));
        serial.disconnectPort(); QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
#endif
    }
    void serialRoundTrip() {
#ifndef Q_OS_UNIX
        QSKIP("Simulation par pseudo-terminal disponible uniquement sous Unix ; tester la carte sous Windows.");
#else
        struct Peer {
            int fd = -1;
            ~Peer() { if (fd >= 0) ::close(fd); }
        } peer;
        peer.fd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        QVERIFY(peer.fd >= 0); QVERIFY(::grantpt(peer.fd) == 0); QVERIFY(::unlockpt(peer.fd) == 0);
        const auto name = QString::fromLocal8Bit(::ptsname(peer.fd));
        SerialController serial(nullptr, settings_.filePath("config.ini")); serial.automatic = false; serial.manualPort = name;
        QSignalSpy systemChanges(&serial, &SerialController::systemStateChanged);
        QSignalSpy state(&serial, &SerialController::stateReceived);
        QSignalSpy error(&serial, &SerialController::deviceError);
        QSignalSpy warning(&serial, &SerialController::deviceWarning);
        QSignalSpy response(&serial, &SerialController::commandResult);
        serial.connectPort(); QTRY_VERIFY_WITH_TIMEOUT(serial.connected(), 3000);
        QCOMPARE(serial.systemState().port, name);
        QCOMPARE(serial.systemState().baud, serial.baud);
        const auto activeBaud = serial.systemState().baud;
        serial.baud = 9600;
        serial.saveSettings();
        QCOMPARE(serial.systemState().baud, activeBaud);
        serial.baud = activeBaud;
        serial.saveSettings();
        QVERIFY(!serial.systemState().initialized);
        QVERIFY(!serial.canSendCommands());
        serial.sendCommand(Protocol::CMD::MesuresOff, {}, true);
        serial.setAcquisition(true);
        QTest::qWait(30);
        char initial[1024];
        QVERIFY(::read(peer.fd, initial, sizeof initial) <= 0);
        QByteArray frame(7, '\0'); frame[0] = '\1'; frame[1] = 'S'; frame[2] = char(250);
        qToLittleEndian<qint32>(3, frame.data() + 3);
        QCOMPARE(::write(peer.fd, frame.constData(), 2), ssize_t(2));
        QTest::qWait(10); QVERIFY(state.isEmpty());
        QCOMPARE(::write(peer.fd, frame.constData() + 2, 5), ssize_t(5));
        QTRY_COMPARE(state.size(), 1); QCOMPARE(state[0].size(), 1); QCOMPARE(state[0][0].toInt(), 3);
        QVERIFY(!serial.canSendCommands());
        frame[1] = 'I'; frame[2] = 71;
        QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
        QTRY_VERIFY(serial.canSendCommands());
        QVERIFY(serial.systemState().initialized);
        auto reportStatus = [&](char type, quint8 code, qint32 value) {
            frame[1] = type; frame[2] = char(code);
            qToLittleEndian<qint32>(value, frame.data() + 3);
            return ::write(peer.fd, frame.constData(), frame.size());
        };
        QByteArray measurement(24, '\0'); measurement[0] = '\1'; measurement[1] = 'M';
        QByteArray acceleration(24, '\0'); acceleration[0] = '\1'; acceleration[1] = 'A';
        QCOMPARE(::write(peer.fd, measurement.constData(), measurement.size()), ssize_t(24));
        QCOMPARE(::write(peer.fd, acceleration.constData(), acceleration.size()), ssize_t(36));
        QTRY_VERIFY(serial.systemState().measuring && serial.systemState().accelerometer);
        QCOMPARE(reportStatus('O', 150, 0), ssize_t(7));
        QCOMPARE(reportStatus('O', 53, 0), ssize_t(7));
        QTRY_VERIFY(!serial.systemState().measuring && !serial.systemState().accelerometer);
        QCOMPARE(reportStatus('O', 151, 20), ssize_t(7));
        QTRY_VERIFY(serial.systemState().measuring);
        QCOMPARE(reportStatus('O', 44, 1), ssize_t(7));
        QTRY_VERIFY(serial.systemState().accelerometer);
        const auto unchanged = systemChanges.size();
        QCOMPARE(reportStatus('O', 44, 1), ssize_t(7));
        QCOMPARE(reportStatus('N', 44, 0), ssize_t(7));
        QTest::qWait(30);
        QCOMPARE(systemChanges.size(), unchanged);
        QVERIFY(serial.systemState().accelerometer);
        QCOMPARE(reportStatus('O', 53, 0), ssize_t(7));
        QTRY_VERIFY(!serial.systemState().accelerometer);
        QCOMPARE(reportStatus('O', 150, 0), ssize_t(7));
        QTRY_VERIFY(!serial.systemState().measuring);
        QCOMPARE(reportStatus('O', 151, 20), ssize_t(7));
        QCOMPARE(reportStatus('O', 44, 1), ssize_t(7));
        QTRY_VERIFY(serial.systemState().measuring && serial.systemState().accelerometer);
        QCOMPARE(reportStatus('I', 70, 0), ssize_t(7));
        QTRY_VERIFY(!serial.systemState().initialized);
        QVERIFY(!serial.systemState().measuring && !serial.systemState().accelerometer);
        QCOMPARE(reportStatus('I', 71, 0), ssize_t(7));
        QTRY_VERIFY(serial.systemState().initialized);
        response.clear();
        serial.sendCommand(Protocol::CMD::MesuresOff, {}, true);
        QByteArray sent;
        auto readPeer = [&] {
            char buffer[1024]; auto n = ::read(peer.fd, buffer, sizeof buffer);
            if (n > 0) sent.append(buffer, n);
            return sent.contains("SET MESURES OFF\n");
        };
        QTRY_VERIFY_WITH_TIMEOUT(readPeer(), 2000);
        frame[1] = 'O'; frame[2] = 0; qToLittleEndian<qint32>(0, frame.data() + 3);
        QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
        QTRY_COMPARE(response.size(), 1);
        frame[1] = 'E'; frame[2] = 10;
        QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
        QTRY_COMPARE(error.size(), 1);
        MessageEvent message; message.type = 'E'; message.code = 10; message.value = 0;
        QCOMPARE(error[0][0].toString(), message.content());
        const auto previousWarnings = warning.size();
        frame[1] = 'W';
        QCOMPARE(::write(peer.fd, frame.constData(), frame.size()), ssize_t(7));
        QTRY_COMPARE(warning.size(), previousWarnings + 1);
        message.type = 'W';
        QCOMPARE(warning.last()[0].toString(), message.content());
        QCOMPARE(error.size(), 1);
        serial.setAcquisition(true);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            char b[1024]; auto n = ::read(peer.fd, b, sizeof b); if (n > 0) sent.append(b, n);
            return sent.contains("SET MESURES ON\n");
        })(), 2000);
        QSignalSpy lost(&serial, &SerialController::connectionLost);
        serial.disconnectPort(); QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
        QCOMPARE(lost.size(), 0);
        char buffer[1024]; auto n = ::read(peer.fd, buffer, sizeof buffer); if (n > 0) sent.append(buffer, n);
        QVERIFY(sent.contains("SET MESURES OFF\n"));
        QVERIFY(!serial.connected());
        QVERIFY(!serial.systemState().initialized);
        QVERIFY(!serial.systemState().measuring && !serial.systemState().accelerometer);
        // Reconnexion manuelle puis retrait du périphérique simulé.
        QSignalSpy fault(&serial, &SerialController::fault);
        serial.connectPort(); QTRY_VERIFY_WITH_TIMEOUT(serial.connected(), 3000);
        QVERIFY(!serial.canSendCommands());
        ::close(peer.fd); peer.fd = -1;
        QTRY_VERIFY_WITH_TIMEOUT(!serial.busy(), 3000);
        QVERIFY(!serial.connected()); QVERIFY(!fault.isEmpty());
        QCOMPARE(lost.size(), 1);
#endif
    }
};
QTEST_MAIN(IntegrationTests)
#include "IntegrationTests.moc"
