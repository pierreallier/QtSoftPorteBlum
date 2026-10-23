/**
 * @file PIDWidget.h
 * @brief Sélection d'un correcteur P, PI ou PID et saisie de ses gains.
 */
#pragma once
#include <QWidget>
#include "serial/Protocol.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QToolButton;
class QVBoxLayout;

/**
 * @brief Regroupe l'activation, le type, les gains et l'aide d'un correcteur.
 *
 * Le type sélectionné détermine les gains modifiables : Kp pour P, Kp et Ki pour PI,
 * et les trois gains pour PID. Désactiver le correcteur bloque la saisie sans effacer
 * les valeurs. Les gains exclus par le type sont représentés par zéro dans les commandes
 * et la description, tout en conservant leur valeur saisie pour un changement ultérieur.
 *
 * Le widget construit les commandes ; leur envoi et la cohérence entre les sélections
 * des correcteurs vitesse et position sont gérés par PilotagePage.
 */
class PIDWidget final : public QWidget {
    Q_OBJECT

    public:
        /* @brief Crée un correcteur activé, de type P, avec Kp = 1 et Ki = Kd = 0.
         * @param title Titre affiché dans l'en-tête.
         */
        explicit PIDWidget(const QString &title, QWidget *parent = nullptr);
        bool isChecked() const; // Indique si la case d'activation est cochée.
        void setChecked(bool checked); // Modifie l'activation.
        QString description() const; /// Description textuelle du réglage actuel, incluant le type et les gains.
        /**
         * @brief Construit une liste contenant une commande et ses trois gains Kp, Ki, Kd.
         * @param cmd Commande PID à utiliser : SetPidVitesse ou SetPidPosition.
         *
         * Les valeurs numériques utilisent le point décimal du protocole.
         * La commande est produite même si le correcteur est décoché ; seul le type
         * sélectionné détermine quels gains sont remplacés par zéro.
         */
        QVector<Protocol::CommandRequest> getCommand(Protocol::CMD cmd) const;

    signals:
        void toggled(bool checked);
        
    private:
        QVBoxLayout *makeUI(const QString &title); // Construit l'en-tête et la grille alignant la liste des types et les champs de gains.
        void makeHelp(); // Recalcule l'infobulle : formule du correcteur et définition des gains du type sélectionné.
        void updateControls(); // Synchronise les champs et leurs libellés avec l'activation et le type, puis actualise l'aide.

        QCheckBox *active_; // Activation du correcteur, indépendante des valeurs de gains.
        QComboBox *type_; // Types P, PI et PID, dans cet ordre.
        QDoubleSpinBox *kp_, *ki_, *kd_; // Gains entiers compris entre 0 et 100.
        QLabel *kiLabel_, *kdLabel_; // Libellés désactivés lorsque le gain correspondant est indisponible.
        QToolButton *help_; // Bouton affichant l'aide au survol ou au clic.
};
