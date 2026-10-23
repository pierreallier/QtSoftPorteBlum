#include "gui/widgets/SerialWidget.h"
#include "gui/components/StyleHelpers.h"
#include "gui/components/StatusIndicator.h"
#include <QHBoxLayout>
#include <QPushButton>
#include "core/AppSettings.h"
#include "serial/SerialController.h"
#include <memory>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainter>

namespace {
// Le bouton affiche uniquement son texte, dans la couleur du voyant associé.
class PortStatusButton final : public QPushButton {
public:
    explicit PortStatusButton(StatusIndicator *indicator) : indicator_(indicator) {}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(indicator_->palette().color(indicator_->isEnabled() ? QPalette::Active : QPalette::Disabled,
                                                   QPalette::WindowText));
        painter.drawText(contentsRect().adjusted(4, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, text());
    }
private:
    StatusIndicator *indicator_;
};
}

QDialog *SerialWidget::makeDialog() {
    // Create a dialog for configuring serial connection settings.
    auto dialog = new QDialog(this);
    dialog->setObjectName("serialSettingsDialog");
    dialog->setWindowTitle("Connexion Arduino");
    auto form = new QFormLayout(dialog);
    form->setContentsMargins(24, 24, 24, 24);
    form->setSpacing(16);
    auto automatic = new QCheckBox("Recherche et connexion automatique"); automatic->setChecked(serial_->automatic);
    automatic->setToolTip("Après une demande de connexion, recherche et reconnexion automatiques. Au démarrage : scan seulement.");
    auto ports = new QComboBox; ports->setEditable(true);
    ports->setFocusPolicy(Qt::TabFocus);
    for (const auto &p : serial_->ports()) ports->addItem(p.portName());
    ports->setCurrentText(serial_->manualPort.isEmpty() ? serial_->selectedPort() : serial_->manualPort);
    auto baud = new QComboBox; baud->setEditable(true);
    baud->setFocusPolicy(Qt::TabFocus);
    for (int value : {9600, 19200, 38400, 57600, 115200, 230400, 250000, 500000, 1000000}) baud->addItem(QString::number(value));
    baud->setCurrentText(QString::number(serial_->baud));
    form->addRow(automatic); form->addRow("Port COM", ports); form->addRow("Débit (bauds)", baud);
    auto toggle = [ports, baud](bool on) {
        ports->setEnabled(!on);
        baud->setEnabled(!on);
        if (on) baud->setCurrentText(AppSettings::defaultValue("serial/baud").toString());
    };
    toggle(automatic->isChecked());
    auto note = new QLabel("Modifier le port, le débit ou le mode de connexion déconnectera la carte."); form->addRow(note);
    note->setTextFormat(Qt::PlainText);
    note->setWordWrap(true);
    note->setMaximumWidth(420);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); form->addRow(buttons);
    auto updateSaveEnabled = [automatic, ports, buttons] {
        buttons->button(QDialogButtonBox::Save)->setEnabled(automatic->isChecked() || !ports->currentText().trimmed().isEmpty());
    };
    QObject::connect(ports, &QComboBox::currentTextChanged, dialog, updateSaveEnabled);
    QObject::connect(automatic, &QCheckBox::toggled, dialog, toggle);
    QObject::connect(automatic, &QCheckBox::toggled, dialog, updateSaveEnabled);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
                     [this, dialog, automatic, ports, baud, note] {
        if (saveSerialSettings(automatic->isChecked(), ports->currentText(), baud->currentText(), note))
            dialog->accept();
    });

    updateSaveEnabled();
    return dialog;
}

void SerialWidget::showDialog() {
    if (!debugMode_) return;
    std::unique_ptr<QDialog> dialog(makeDialog());
    dialog->exec();
}

bool SerialWidget::saveSerialSettings(bool automatic, const QString &port, const QString &baud, QLabel *note) {
    bool ok = false;
    const auto value = automatic ? AppSettings::defaultValue("serial/baud").toInt(&ok) : baud.toInt(&ok);
    if (!ok || value <= 0) {
        note->setText(tr("Débit invalide"));
        return false;
    }
    const auto selectedPort = port.trimmed();
    if (!automatic && selectedPort.isEmpty()) {
        note->setText(tr("Choisissez un port"));
        return false;
    }
    QString error;
    if (!serial_->applySerialSettings(automatic, automatic ? serial_->manualPort : selectedPort, value, &error)) {
        note->setText(error);
        return false;
    }
    return true;
}

SerialWidget::SerialWidget(SerialController *serial, QWidget *parent) : QWidget(parent), serial_(serial) {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(8);
    indicator_ = new StatusIndicator;
    indicator_->setObjectName("serialIndicator");
    status_ = new PortStatusButton(indicator_);
    status_->setFocusPolicy(Qt::NoFocus);
    status_->setObjectName("serialwidget");
    status_->setFlat(true);
    layout->addWidget(indicator_);
    layout->addWidget(status_);
    connect(status_, &QPushButton::clicked, this, &SerialWidget::showDialog);
    connect(serial_, &SerialController::portsChanged, this, &SerialWidget::updateConnection);
    connect(serial_, &SerialController::connectionChanged, this, &SerialWidget::updateConnection);
    updateConnection();
}
SerialWidget::ConnectionState SerialWidget::currentState() const {
    if (serial_->connected()) return ConnectionState::Connected;
    return serial_->selectedPort().isEmpty() ? ConnectionState::Unknown : ConnectionState::Disconnected;
}

void SerialWidget::updateConnection() {
    const auto connection = currentState();
    const auto port = serial_->selectedPort();
    const bool on = connection == ConnectionState::Connected;
    const QString state = on ? "connected" : connection == ConnectionState::Unknown ? "unknown" : "disconnected";
    status_->setText(port.isEmpty() ? tr("Carte non détectée") : tr("%1 — %2").arg(on ? tr("Connecté") : tr("Non connecté"), port));
    setStyleProperty(this, "state", state);
    indicator_->setEnabled(connection != ConnectionState::Unknown);
    indicator_->setState(on ? StatusIndicator::State::Ok : StatusIndicator::State::Error);
    indicator_->setToolTip(status_->text());
    status_->update();
}
