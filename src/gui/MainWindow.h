/**
 * @file MainWindow.h
 * @brief Fenêtre principale et coordination des pages avec le contrôleur série.
 */
#pragma once
#include "gui/widgets/Page.h"
#include <array>
#include "serial/SerialController.h"
#include <QTimer>
#include <QMainWindow>
#include <QTabWidget>
#include <QElapsedTimer>
#include <QSet>

class SerialWidget;
class StatusWidget;
class TwoStateButton;
class VisuPage;
class QCloseEvent;
class QPushButton;

/**
 * @brief Regroupe les pages Mesures, Pilotage et Debug et les contrôles communs.
 *
 * Distribue les événements à toutes les pages et les mesures à la page active
 * ainsi qu'à Debug, même lorsque cet onglet est masqué. Le passage entre Mesures
 * et Pilotage attend la confirmation readyToExit() de la page quittée.
 * Gère aussi les fenêtres de visualisation CSV et la géométrie de la fenêtre.
 */
class MainWindow final : public QMainWindow {
    Q_OBJECT

    public:
        /**
         * @brief Construit l'interface et démarre la distribution périodique des données.
         * @param parent Parent Qt de la fenêtre.
         * @param serial Contrôleur à utiliser, qui conserve son propriétaire actuel.
         * Si nullptr, un contrôleur appartenant à cette fenêtre est créé.
         */
        explicit MainWindow(QWidget *parent = nullptr, SerialController *serial = nullptr);

        ~MainWindow() override; // Détruit les fenêtres de visualisation encore ouvertes.
        /**
         * @brief Relie les commandes et notifications d'une page au contrôleur série.
         * Initialise également son état système et relie sa confirmation de sortie.
         * @param page Page non nulle ; sa propriété et son inscription dans pages_
         * ne sont pas modifiées.
         */
        void connectPage(Page *page);

    public slots:
        void showError(const QString &title, const QString &message); // affiche un message d'erreur

    protected:
        void closeEvent(QCloseEvent *event) override; // Gestion fermeture de la fenêtre principale et de toutes les fenêtres de visualisation ouvertes.

    private:
        // Construction de l'interface.
        QWidget *createTab(); // Construit les onglets (Pages) et connections
        QWidget *createFooter(); // Construit le pied de fenêtre : port série, état et bouton de réinitialisation.

        // Connexion série et distribution des données.
        void updateSerialData(); // Synchronise les boutons de connexion et de reset avec l'état du contrôleur.
        void toggleSerialConnection(bool on); // Demande la connexion si on vaut true, sinon la déconnexion du port série.
        void dispatchEvents(); // Distribue les nouveaux évènements depuis les curseurs, puis avance ce dernier.
        void dispatchMesures(); // Distribue les nouvelles mesures depuis le curseur, puis avance ce dernier.

        // Navigation et visualisation.
        void showDebug(); // Affiche Debug ou déclenche son action principale si déjà actif.
        void showMesure(); // Affiche l'onglet Mesures ou déclenche son action principale si déjà actif.
        void showPilotage(); // Affiche l'onglet Pilotage ou déclenche son action principale si déjà actif.
        void toogleTab(); // Traite le changement d'onglet avec attente de confirmation readyToExit() de la page quittée.
        void onPageReadyToExit(bool success); // Termine le basculement uniquement si success vaut true.
        void toogleDebug(); // Bascule le mode Debug après trois clics sur le logo en une seconde
        void requestOpen(); // Gère l'ouverture d'un fichier CSV dans une nouvelle fenêtre de visualisation.

        // État de la communication
        SerialController *serial_; // Contrôleur fourni au constructeur ou créé comme enfant de cette fenêtre.
        quint64 measureCursor_ = 0, eventCursor_ = 0; // Positions de lecture indépendantes dans les files de mesures et d'événements.
        QTimer poll_; // Timer de distribution périodique des mesures et événements aux pages.

        // Pages et transitions entre onglets.
        enum PageIndex { Measures = 0, Pilotage, Debug, PageCount };
        QTabWidget *tabs_; // Conteneur des onglets
        std::array<Page *, PageCount> pages_{}; // Pages dans l'ordre fixe Mesures, Pilotage, Debug, y compris les pages masquées.
        Page *activePage_ = nullptr; // Page actuellement active (peut être Debug)
        Page *controlPage_ = nullptr; // Page qui est destinataire des mesures (autre que Debug)
        Page *pendingPage_ = nullptr; // Destination en attente de readyToExit(), ou nullptr en l'absence de transition.
        QSet<VisuPage *> visualisationWindows_; // Fenêtres enfant de visualisation de fichiers mesures

        // Contrôles communs.
        TwoStateButton *connection_;
        QPushButton *reset_;
        SerialWidget *serialWidget_;
        StatusWidget *statusWidget_;

        // Activation du mode Debug par triple-clic.
        QElapsedTimer clicksTime_;
        int clicks_ = 0;
        bool debugVisible_ = false;
};
