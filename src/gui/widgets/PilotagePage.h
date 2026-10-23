/**
 * @file PilotagePage.h
 * @brief Interface de configuration du pilotage et d'enregistrement des mesures.
 */
#pragma once
#include "core/EventQueue.h"
#include "gui/widgets/Page.h"
#include <QStringList>
#include <QTimer>
#include <optional>
class QHBoxLayout;
class QProgressDialog;
class GraphWidget;
class PIDWidget;
class QGroupBox;
class ConsigneWidget;
class TwoStateButton;
class CalibrationButton;

/**
 * @brief Assemble les PID, la consigne, les actions et les courbes de pilotage.
 *
 * Les PID et la consigne fournissent leurs commandes et paramètres.
 * La page gère leur envoi séquentiel, leur confirmation, la progression et l'arrêt lors d'un changement
 * d'onglet. L'asservissement impose qu'au moins un des deux PID soit sélectionné.
 *
 * Les nouvelles mesures sont transmises après confirmation de toute la séquence,
 * y compris DO PILOTER. La dernière mesure antérieure est ajoutée au démarrage ;
 * GraphWidget utilise cette première mesure comme origine.
 * L'acquisition est arrêtée automatiquement après dix secondes.
 */

// TODO : gérer les messages de Pilotage (Début, Fin ou Erreur) via états Machine ?

class PilotagePage final : public Page {
    Q_OBJECT

    public:
        /// Mode déduit de l'activation de l'asservissement et des deux sélections PID.
        enum class TypeAsservissement { Aucun, Vitesse, Position, PositionVitesse };
        Q_ENUM(TypeAsservissement)
        
        explicit PilotagePage(QWidget *parent = nullptr);
        void triggerPrimaryAction();
        
        void stopPilotage(); // Interrompt la séquence, stoppe le système si nécessaire et termine l'acquisition locale.
        void requestExit() override; // Ferme propremment la page en stopant le pilotage et l'acquisition locale.
        TypeAsservissement typeAsservissement() const; // Retourne le mode d'asservissement actuellement sélectionné.
        QVector<Protocol::CommandRequest> commandRequests() const; // Construit la séquence des consignes pour piloter le système selon le réglage actuel.

        int controlsWidth() const { return controlsPanel_->width(); } // Largeur du panneau de commandes, utilisée pour l'alignement entre pages.
        void setControlsWidth(int width) { controlsPanel_->setFixedWidth(width); } // Fixe la largeur du panneau de commandes.
    
    public slots:
        void setVisible(bool visible) override;
        void setDebugMode(bool enabled) override;
        void setSystemState(const SystemState &state) override;
        void processMeasurements(const EventQueue::Batch &batch) override;
        void commandFinished(Protocol::CMD command, bool accepted) override;

    private:
        // Construction de l'interface.
        QHBoxLayout* makeUI(); // Assemble les commandes, le séparateur et les courbes.
        QWidget* makeControlsUI(); // Crée les PID, la consigne et les boutons d'action.
        QGroupBox* makeAsservissementUI(); // Crée le groupe activable contenant les deux PID.
        void makeProgressDialog(); // Crée la boite de dialogue de progression avec son action Annuler.

        // Envoi séquentiel de la configuration et suivi de ses confirmations.
        void togglePilotage(bool on); // Lance la configuration ou demande l'arrêt selon le bouton.
        void toggleAccelerometer(bool on); // Demande le changement d'état et bloque le contrôle en attendant.
        void updateProgress(int completed, const QString &command); // Affiche l'avancement et la commande envoyée.
        void sendNextCommand(); // Envoie une commande ou démarre l'acquisition lorsque la file est vide.
        void failCommand(); // Arrête la séquence et affiche l'échec de la commande en attente.

        // Acquisition locale des mesures moteur et transmission au graphique.
        void startAcquisition(); // Démarre l'acquisition locale et la transmission des mesures moteur au graphique.
        QString modeDescription() const; // Décrit l'asservissement et ses PID pour les métadonnées d'enregistrement.

        // Sélection des PID et disponibilité des commandes.
        void updateSelection(); // Réévalue la sélection après activation ou désactivation de l'asservissement.
        void updatePidSelection(PIDWidget *changed); // Garantit un PID actif et adapte limites, unité et échelle de consigne.
        void speedSelectionChanged(); // Réévalue la sélection après modification du PID vitesse.
        void positionSelectionChanged(); // Réévalue la sélection après modification du PID position.
        void setRunning(bool running); // Synchronise le bouton et verrouille les paramètres pendant la séquence.
        void updateButtons(); // Adapte les actions à la connexion, à la calibration et à la sortie d'onglet.

        // Termine la sequence et l'acquisition avant de signaler readyToExit(true).
        void finishRun();

        QWidget *controlsPanel_; // Panneau gauche dont la largeur est partagée avec la page Mesures.
        TwoStateButton *send_; // Action Envoyer / Stopper.
        CalibrationButton *calibrate_; // Action de calibration avec confirmation intégrée.
        GraphWidget *curves_; // Historique brut, correction de l'origine et affichage des courbes.
        QGroupBox *asservissement_; // Activation globale de l'asservissement.
        PIDWidget *speed_, *position_; // Sélections et paramètres des deux PID.
        ConsigneWidget *consigne_; // Forme et paramètres de la consigne à envoyer.
        QProgressDialog *progress_ = nullptr; // Dialogue créé au premier lancement.
        QVector<Protocol::CommandRequest> commands_; // Commandes restant à envoyer, dans l'ordre.
        std::optional<Protocol::CommandRequest> pending_; // Commande en cours, jusqu'à confirmation.
        int completedCommands_ = 0; // Nombre de commandes terminées pour la progression.

        // Acquisition de pilotage de dix secondes.
        EventQueue::Batch recentMeasurements_; // Dernière mesure moteur, ajoutée au démarrage.
        QTimer acquisitionTimer_; // Arrêt automatique après dix secondes d'acquisition locale.
        bool acquiring_ = false; // Autorise la transmission des nouveaux lots au graphique.
        bool running_ = false; // Séquence active, depuis l'envoi de la configuration jusqu'à l'arrêt.
};
