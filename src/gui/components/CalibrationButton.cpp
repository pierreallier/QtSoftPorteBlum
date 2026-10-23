#include "gui/components/CalibrationButton.h"
#include <QIcon>
#include <QMessageBox>

CalibrationButton::CalibrationButton(QWidget *parent)
    : QPushButton(tr("Calibration"), parent) {
    setObjectName("boutonCalibration");
    setIcon(QIcon(":/icons/calibrate.svg"));
    setToolTip(tr("Calibrer le système"));
    connect(this, &QPushButton::clicked, this, &CalibrationButton::showConfirmation);
}

bool CalibrationButton::showConfirmation() {
    if (!isEnabled()) return false;
    QMessageBox confirmation(QMessageBox::Warning, tr("Confirmation calibration"), tr("<b>Attention, sécurités désactivées !</b>"),
                             QMessageBox::Yes | QMessageBox::No, this);
    confirmation.setTextFormat(Qt::RichText);
    confirmation.setInformativeText(tr(
        "<ul>"
        "<li>La porte peut-elle s'ouvrir sans blocage ?</li>"
        "<li>Voulez-vous démarrer la calibration ?</li>"
        "</ul>"
    ));
    confirmation.setDefaultButton(QMessageBox::No);
    if (confirmation.exec() != QMessageBox::Yes) return false;
    emit sendCommand(Protocol::CMD::Calibration);
    return true;
}
