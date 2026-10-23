/**
 * @file StateGraphWidget.h
 * @brief Affichage interactif des machines à états de production et de calibration.
 */
#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QMap>
#include <QPainterPath>
#include <QColor>
#include <QTimer>
#include <QQueue>
class QLabel;

/**
 * @brief Dessine les états et transitions définis dans la configuration des machines.
 *
 * La machine affichée dépend des conditions display_when et de la machine par défaut.
 * Les changements d'état sont mis en file et affichés à au moins 100 ms d'intervalle ;
 * les notifications répétant le dernier état de la même machine sont ignorées.
 * Un clic sur un état associé à une commande émet sendRawCommand si les commandes
 * sont autorisées. Le widget n'envoie lui-même aucune commande sur la liaison série.
 *
 * Le panneau de débogage affiche l'état de chaque machine. Les propriétés QSS
 * règlent les couleurs, l'épaisseur des contours et le gras de l'état actif.
 */
class StateGraphWidget final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor stateBorderColor MEMBER stateBorderColor_)
    Q_PROPERTY(QColor stateBackgroundColor MEMBER stateBackgroundColor_)
    Q_PROPERTY(QColor activeBorderColor MEMBER activeBorderColor_)
    Q_PROPERTY(QColor activeBackgroundColor MEMBER activeBackgroundColor_)
    Q_PROPERTY(QColor hoverBorderColor MEMBER hoverBorderColor_)
    Q_PROPERTY(QColor hoverBackgroundColor MEMBER hoverBackgroundColor_)
    Q_PROPERTY(qreal borderWidth MEMBER borderWidth_)
    Q_PROPERTY(bool activeTextBold MEMBER activeTextBold_)

    public:
        /**
         * @brief Charge les définitions, construit le panneau de débogage et initialise les états.
         * @param parent Widget parent propriétaire.
         * @param definitionsPath Ressource ou fichier JSON ; la ressource par défaut utilise AppSettings.
         * @param calibrationStateThreshold Seuil de routage de la surcharge setState(int).
         */
        explicit StateGraphWidget(QWidget *parent = nullptr, const QString &definitionsPath = ":/machines.json",
                                int calibrationStateThreshold = 20);
        /// Code de la première machine dont la condition d'affichage est satisfaite, sinon code par défaut.
        int currentMachineCode() const;
        /// Nom complet de l'état actif de la machine affichée, ou « inconnu ».
        QString activeStateName() const;
        /// Description « code : nom » de l'état d'une machine, ou « inconnu » si elle n'a pas d'état actif.
        QString stateDescription(int machineCode) const;
        /// Route les états sous le seuil vers la production (250), les autres vers la calibration (251).
        void setState(int state);
        /// Met en file un état pour la machine indiquée ; ignore les répétitions de son dernier état connu ou en attente.
        void setState(int machineCode, int state);
        /// Annule les changements en attente et restaure les default_state définis dans la configuration.
        void reset();
        /// Affiche ou masque le panneau de débogage et recalcule l'espace disponible pour le graphe.
        void setDebugMode(bool enabled);

    signals:
        /// Demande une commande brute après un clic gauche sur un état interactif.
        void sendRawCommand(const QString &command);

    protected:
        /// Efface le survol lorsque le widget est active ou desactive.
        void changeEvent(QEvent *event) override;
        /// Dessine les transitions et les nœuds, avec les styles actif et survolé.
        void paintEvent(QPaintEvent *) override;
        /// Émet la commande du nœud sous le pointeur lors d'un relâchement du bouton gauche.
        void mouseReleaseEvent(QMouseEvent *event) override;
        /// Actualise le survol et le curseur selon la présence d'une commande autorisée.
        void mouseMoveEvent(QMouseEvent *event) override;
        /// Efface le survol et restaure le curseur à la sortie du widget.
        void leaveEvent(QEvent *event) override;
    private:
        /// Ajoute en haut à gauche un bouton d'aide expliquant les commandes par clic sur les états.
        void makeHelpUI();
        /// Construit le panneau et ses libellés par machine ; retourne le panneau initialement masqué.
        QWidget *makeDebugUI();
        QWidget *debugPanel_; ///< Panneau de débogage, enfant du widget.
        QMap<int, QLabel *> debugLabels_; ///< Libellés indexés par code de machine.
        /// Met à jour les libellés à partir des états actuellement affichés.
        void updateDebugStates();
        /// Géométrie et présentation calculées pour un état visible de la machine courante.
        struct Node {
            int code; ///< Identifiant de l'état.
            QString name; ///< Libellé affiché, éventuellement remplacé par alt.
            QString fullName; ///< Nom complet provenant de la configuration.
            QString type; ///< Type de nœud, notamment start ou end pour les cercles.
            QString command; ///< Commande brute associée, vide si le nœud n'est pas interactif.
            QPointF center; ///< Centre dans les coordonnées du widget.
            QPainterPath shape; ///< Forme utilisée pour le dessin et la détection des clics.
            QFont font; ///< Police ajustée à l'espace disponible.
        };
        QJsonArray machines_; ///< Définitions des machines, de leurs états et de leurs transitions.
        int defaultMachineCode_ = -1; ///< Machine de repli en l'absence de condition satisfaite.
        int calibrationStateThreshold_; ///< Seuil utilisé par le routage implicite des états.
        QMap<int, int> active_; ///< État actuellement affiché pour chaque code de machine.
        QTimer stateTimer_; ///< Temporisation de 50 ms entre deux changements affichés.
        QQueue<QPair<int, int>> pendingStates_; ///< File des couples (code de machine, état).
        /// Applique le prochain état en attente et relance la temporisation.
        void showNextState();
        /// Calcule les nœuds non masqués et leur géométrie pour la machine courante.
        QList<Node> nodes() const;
        /// Zone de dessin disponible, hors panneau de débogage lorsqu'il est visible.
        QRectF graphArea() const;
        /// Hauteur des états rectangulaires selon etat_height, avec un minimum d'un pixel.
        qreal stateHeight() const;
        /// Diamètre des états circulaires selon etat_diameter, avec un minimum d'un pixel.
        qreal stateDiameter() const;
        /// Lit un ratio de la machine courante, borné entre 0.001 et 1 ; utilise fallback si absent.
        qreal machineRatio(const QString &key, qreal fallback) const;
        /// Cherche la plus grande police tenant dans available ; maxPixelSize limite la taille s'il est positif.
        QFont graphFont(const QString &text, const QSizeF &available, int maxPixelSize) const;
        /// Commande brute du nœud sous position, ou chaîne vide si aucune commande n'est disponible.
        QString commandAt(const QPointF &position) const;
        /// Nom complet de l'état affiché pour une machine, ou « inconnu ».
        QString stateName(int machineCode) const;

        QPointF hoverPosition_; ///< Dernière position du pointeur dans le widget.
        bool hovering_ = false; ///< Indique si le pointeur est dans le widget.
        qreal borderWidth_ = 2; ///< Épaisseur des contours, réglable par QSS.
        bool activeTextBold_ = true; ///< Affichage en gras de l'état actif, réglable par QSS.
        QColor stateBorderColor_{"#64748B"}, stateBackgroundColor_{"#F1F5F9"}; ///< Couleurs normales.
        QColor activeBorderColor_{"#F75A1B"}, activeBackgroundColor_{"#FDEAE2"}; ///< Couleurs de l'état actif.
        QColor hoverBorderColor_{"#F75A1B"}, hoverBackgroundColor_{"#FDEAE2"}; ///< Couleurs du survol interactif.
};
