#pragma once
#include <QPushButton>
#include "serial/Protocol.h"

/**
 * @brief Bouton de calibration du système.
 *
 * Après confirmation, émet sendCommand avec la clé "Calibration" (DO CALIBRATION).
 * La page relaie ce signal. Ce composant ne connaît ni la page ni le port série.
 * La page propriétaire contrôle son activation selon l'état de l'application.
 */
class CalibrationButton final : public QPushButton {
    Q_OBJECT

    public:
        /// Configure le libellé, l'icône et le clic ; parent possède le bouton selon les règles Qt.
        explicit CalibrationButton(QWidget *parent = nullptr);

    public slots:
        /**
         * @brief Ouvre la confirmation et émet la demande de commande après Oui.
         * Avertit que les sécurités sont désactivées et demande de vérifier
         * l'ouverture libre de la porte. Le choix par défaut est Non.
         * @return true après Oui ; false si le bouton est désactivé, après Non ou fermeture.
         */
        bool showConfirmation();

    signals:
        void sendCommand(Protocol::CMD command, const QStringList &values = {}, bool awaitReply = false);
};
