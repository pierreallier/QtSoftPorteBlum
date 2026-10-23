#pragma once

#include <QString>
#include <QMetaType>

/**
 * @brief État maintenu par SerialController dans le thread GUI et transmis aux vues.
 *
 * Les widgets lisent cet instantané ; ils demandent les actions au contrôleur
 * plutôt que de modifier directement ses champs. Cette structure ne possède
 * ni verrou ni logique de transition : SerialController assure la cohérence.
 * Il désactive initialized hors connexion, puis les acquisitions et la
 * calibration lorsque l'initialisation n'est pas terminée.
 */

struct SystemState {
    QString port; // Nom du port retenu pour la connexion ; ne prouve pas qu'il est ouvert.
    qint32 baud = 115200; // Débit série associé à la connexion, en bauds.
    bool connected = false; // Port série ouvert, sans garantie d'initialisation de la carte.
    bool initialized = false; // Fin d'initialisation confirmée sur la connexion courante.
    bool measuring = false; // Acquisition moteur/porte active selon les trames et réponses reçues.
    bool calibration = false; // Indicateur de calibration mis à jour à partir des états reçus.
    bool accelerometer = false; // Acquisition inertielle active ; n'indique pas la présence du capteur.

    // Autorise les commandes utilisateur uniquement si le port est ouvert et la carte initialisée.
    bool canSendCommands() const { return connected && initialized; }

    // Compare tous les champs pour détecter un changement d'instantané.
    bool operator==(const SystemState &other) const {
        return port == other.port && baud == other.baud && connected == other.connected
            && initialized == other.initialized && measuring == other.measuring
            && accelerometer == other.accelerometer && calibration == other.calibration;
    }

    // Vrai si au moins un champ diffère.
    bool operator!=(const SystemState &other) const { return !(*this == other); }
};

// Déclare le type auprès du système de métatypes Qt pour son transport dans les signaux/slots.
Q_DECLARE_METATYPE(SystemState)
