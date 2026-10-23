/**
 * @file MeasurePage.h
 * @brief Page d'acquisition des mesures et de suivi des états du système.
 */
#pragma once
#include "core/MeasurementStore.h"
#include "gui/widgets/Page.h"
class StateGraphWidget;
class GraphWidget;
class TwoStateButton;
class CalibrationButton;
class QLabel;
class QHBoxLayout;

/**
 * @brief Regroupe les commandes d'acquisition et de calibration, le graphe d'états et les courbes.
 *
 * MainWindow transmet l'état du système et les lots d'événements et de mesures.
 * Les StateEvent sont confiés au graphe, qui détermine la machine concernée.
 * Les nouveaux lots de mesures sont ajoutés directement aux courbes.
 * Les demandes de commande sont émises via les signaux hérités de Page.
 */
class MeasurePage final : public Page {
    Q_OBJECT

    public:
        explicit MeasurePage(QWidget *parent = nullptr);
        void requestExit() override;
        void triggerPrimaryAction();
        // Largeur du panneau de commandes, utilisée pour l'alignement avec la page Pilotage.
        int controlsWidth() const { return controlsPanel_->width(); }
        // Fixe la largeur du panneau ; les boutons et le graphe s'adaptent horizontalement.
        void setControlsWidth(int width) { controlsPanel_->setFixedWidth(width); }

    public slots:
        void setVisible(bool visible) override;
        void setDebugMode(bool enabled) override;
        // Mémorise l'état, synchronise les contrôles et réinitialise le graphe si la connexion ou le reset change.
        void setSystemState(const SystemState &state) override;
        // Transmet au graphe uniquement les événements de changement d'état.
        void processEvents(const EventQueue::Batch &batch) override;
        // Ajoute les mesures nouvelles aux courbes en conservant le comptage des pertes.
        void processMeasurements(const EventQueue::Batch &batch) override;
        // Resynchronise le contrôle d'accéléromètre à la fin de sa commande, acceptée ou refusée.
        void commandFinished(Protocol::CMD command, bool accepted) override;
        
    private:
        QWidget *controlsPanel_; // Panneau gauche : boutons en grille et graphe extensible en hauteur.
        TwoStateButton *acquire_; // Commande Acquérir / Arrêter, synchronisée avec SystemState.
        CalibrationButton *calibrate_; // Commande de calibration avec confirmation intégrée.
        StateGraphWidget *graph_; // États de production et de calibration et commandes associées.
        GraphWidget *curves_; // Courbes moteur et accéléromètre.

        QHBoxLayout* makeUI(); // Crée la disposition principale : commandes, séparateur et courbes.
        QWidget* makeControlsUI(); // Crée le panneau gauche
        void toggleAcquisition(bool on); // Demande le démarrage ou l'arrêt des mesures
        void toggleAccelerometer(bool on); // Demande le changement d'état de l'accéléromètre
};
