#pragma once
/** @file
 * @brief Lancement et arrêt du splash animé dans un processus auxiliaire.
 */
#include <QProcess>
#include <QSharedMemory>

/**
 * @brief Gère la durée de vie du processus affichant le splash de démarrage.
 *
 * Le même exécutable est lancé avec --startup-splash et une clé de mémoire
 * partagée unique. Sa boucle d'événements indépendante anime le spinner même
 * lorsque le thread GUI principal est occupé par l'initialisation.
 * Le lancement est asynchrone ; la destruction signale la fin du chargement.
 * L'objet doit rester vivant pendant toute la phase d'initialisation.
 */
class SplashProcess final {
    public:
        /// Prépare le drapeau partagé et lance l'auxiliaire sans attendre l'affichage de sa fenêtre.
        SplashProcess();
        /// Appelle finish() pour ne pas laisser l'auxiliaire actif après l'initialisation.
        ~SplashProcess();
        /**
         * @brief Signale la fin du chargement et attend l'arrêt de l'auxiliaire.
         * Peut être appelée plusieurs fois. Attend au plus 1500 ms pour une sortie
         * normale, puis force l'arrêt si nécessaire et attend encore au plus 1000 ms.
         * Le drapeau empêche aussi un auxiliaire démarré tardivement d'afficher sa fenêtre.
         */
        void finish();
        
    private:
        QSharedMemory state_; ///< Un octet partagé : 0 pendant le chargement, 1 lorsqu'il est terminé.
        QProcess process_; ///< Auxiliaire possédé par cet objet ; sa console est masquée sous Windows.
        bool finishing_ = false; ///< Évite de signaler l'arrêt volontaire comme une indisponibilité du splash.
};

/**
 * @brief Point d'entrée de l'auxiliaire, après création de QApplication.
 * Attache la mémoire partagée, affiche le splash et exécute sa propre boucle
 * d'événements jusqu'au signal de fin. Écrit « ready » sur stdout après show().
 * @param sharedMemoryKey Clé créée par SplashProcess dans le processus principal.
 * @return 2 si l'attachement échoue, 0 si le chargement est déjà terminé,
 * sinon le code de sortie de la boucle QApplication.
 */
int runStartupSplash(const QString &sharedMemoryKey);
