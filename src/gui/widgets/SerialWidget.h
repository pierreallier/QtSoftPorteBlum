/**
 * @file SerialWidget.h
 * @brief Affichage de la connexion série et dialogue de configuration du port.
 */
#pragma once
#include <QWidget>
class SerialController;
class QDialog;
class QLabel;
class StatusIndicator;
class QPushButton;

/**
 * @brief Regroupe le voyant de connexion et le bouton de configuration série.
 *
 * Lit directement SerialController et actualise l'affichage lors de ses signaux
 * portsChanged et connectionChanged. La propriété dynamique QSS "state" vaut
 * "connected", "disconnected" ou "unknown" selon currentState().
 *
 * Le bouton affiche uniquement son texte, sans fond ni bordure, et reprend
 * la couleur du StatusIndicator : vert si connecté, rouge si déconnecté,
 * gris si aucun port n'est sélectionné. Ces couleurs sont définies dans le QSS
 * du voyant. Un clic ouvre le dialogue si les réglages sont autorisés.
 *
 * La sauvegarde et la déconnexion éventuelle sont déléguées au contrôleur.
 */
class SerialWidget final : public QWidget {
    Q_OBJECT
    public:
        // États de connexion, indépendants de l'initialisation de la carte.
        enum class ConnectionState { Connected, Disconnected, Unknown };
        Q_ENUM(ConnectionState)

        /**
         * @brief Construit le widget et initialise l'affichage depuis le contrôleur.
         * @param serial Contrôleur série dont l'état est affiché et modifié par le dialogue.
         */
        explicit SerialWidget(SerialController *serial, QWidget *parent = nullptr);

        // Autorise l'ouverture du dialogue ; désactivée par défaut.
        void setDebugMode(bool enabled) { debugMode_ = enabled; }

        // Déduit l'état courant du contrôleur
        ConnectionState currentState() const;

    public slots:
        void showDialog(); // Crée et affiche un dialogue modal

    private:
        QDialog *makeDialog(); // Construit le dialogue sans l'afficher

        /**
         * @brief Valide la saisie et demande au contrôleur de sauvegarder les réglages.
         * @param automatic Active la sélection automatique avec le débit par défaut.
         * @param port Nom du port manuel ; obligatoire hors mode automatique.
         * @param baud Débit saisi sous forme de texte ; ignoré en mode automatique.
         * @param note Label non nul recevant le message d'erreur éventuel.
         * @return true si les paramètres sont appliqués, false si le dialogue doit rester ouvert.
         *
         * Un changement de port, de débit ou de mode demande la déconnexion après
         * sauvegarde réussie. Une erreur de validation ou de sauvegarde conserve la liaison.
         */
        bool saveSerialSettings(bool automatic, const QString &port, const QString &baud, QLabel *note);

        void updateConnection(); // actualise le widget

        SerialController *serial_;
        StatusIndicator *indicator_; // Voyant circulaire possédé par le widget.
        QPushButton *status_; // Bouton possédé par le widget
        bool debugMode_ = false; // Autorisation d'ouvrir le dialogue
};
