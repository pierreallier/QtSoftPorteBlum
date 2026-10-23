/**
 * @file Page.h
 * @brief Interface commune des pages de l'application et de leurs commandes.
 */
#pragma once
#include "core/EventQueue.h"
#include "core/SystemState.h"
#include "serial/Protocol.h"
#include <QWidget>

/**
 * @brief Base des pages Mesures, Pilotage et Debug.
 *
 * MainWindow distribue les événements à toutes les pages et les mesures à la
 * page active ainsi qu'aux pages d'observation, même cachées. MainWindow gère
 * les curseurs de lecture et transmet uniquement les nouvelles entrées.
 * Les pages émettent leurs demandes de commande telles quelles ; SerialController
 * décide de l'envoi effectif. Cette classe ne filtre pas les commandes.
 *
 * Les classes filles surchargent les slots utiles à leur affichage et adaptent
 * leurs contrôles à SystemState. Les traitements par défaut sont vides, sauf
 * setSystemState qui mémorise le dernier état reçu.
 */
class Page : public QWidget {
    Q_OBJECT

    public:
        /// Construit la page ; parent en devient propriétaire selon les règles Qt.
        explicit Page(QWidget *parent = nullptr) : QWidget(parent) {}
        
        /// Retourne le dernier état mémorisé, initialisé par défaut avant réception.
        const SystemState &systemState() const { return state_; }
    
        // Demande à la page de se préparer à se fermer, en attendant sa confirmation.
        virtual void requestExit() { emit readyToExit(true); }

    public slots:
        void setVisible(bool visible) override { QWidget::setVisible(visible);}

        /// Transmet le mode debug aux composants concernés ; aucun effet par défaut.
        virtual void setDebugMode(bool enabled) { Q_UNUSED(enabled); }

        /**
         * @brief Mémorise l'état du système sans émettre de signal.
         * Les surcharges appellent cette implémentation avant d'actualiser les contrôles.
         * L'état contient notamment l'initialisation de la carte et la calibration en cours.
         */
        virtual void setSystemState(const SystemState &state) { state_ = state; }
        
        /// Traite un lot d'événements transmis par MainWindow ; aucun effet par défaut.
        virtual void processEvents(const EventQueue::Batch &) {}

        /// Traite un lot de mesures transmis par MainWindow ; aucun effet par défaut.
        virtual void processMeasurements(const EventQueue::Batch &) {}
        /**
         * @brief Reçoit le résultat d'une commande ; aucun effet par défaut.
         * @param command Type de requête concerné, sans les valeurs envoyées.
         * @param accepted true si la commande a été acceptée, false sinon.
         */
        virtual void commandFinished(Protocol::CMD command, bool accepted) { Q_UNUSED(command); Q_UNUSED(accepted); }
    
    signals:
        void readyToExit(bool success);

        /**
         * @brief Demande l'envoi d'une commande structurée via MainWindow.
         * @param command Type de commande défini dans le protocole.
         * @param values Valeurs optionnelles, conservées dans leur ordre d'envoi.
         * @param awaitReply Indique si l'envoi doit attendre une réponse du dispositif.
         */
        void sendCommand(Protocol::CMD command, const QStringList &values = {}, bool awaitReply = false);
        
        /**
         * @brief Demande l'envoi d'un texte de commande brut via MainWindow.
         * L'envoi n'attend jamais de réponse. La page transmet le texte sans le modifier.
         */
        void sendRawCommand(const QString &command);
    
    private:
        /// Instantané local de l'état communiqué par MainWindow.
        SystemState state_;
};
