#pragma once
#include <QPushButton>
#include <array>

/**
 * @brief Bouton à deux états avec un texte et une icône propres à chaque état.
 *
 * Un clic émet le signal natif toggled(bool), à connecter au traitement voulu.
 * Mais il ne change pas d'état tant que l'on valide le traitement via le slot 
 * setChecked(bool).
 *
 * La présentation reste synchronisée. La largeur couvre les deux présentations 
 * pour éviter les déplacements du layout lors d'un basculement.
 */
class TwoStateButton final : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(bool state READ state WRITE setChecked)

    public:
        /** @brief Crée un bouton initialement état non coché = false.
         * @param parent Widget propriétaire selon les règles de durée de vie Qt.
         */
        explicit TwoStateButton(QWidget *parent = nullptr);

        /** @brief Définit le libellé d'un état et met à jours le bouton.
         * @param state false pour l'état non coché, true pour l'état coché.
         * @param text Texte affiché pour cet état.
         * Ne change pas l'état et n'émet pas toggled(bool).
         */
        void setStateText(bool state, const QString &text);

        /** @brief Définit l'icône d'un état et met à jours le bouton.
         * @param state false pour l'état non coché, true pour l'état coché.
         * @param icon Icône affichée ; une icône vide supprime son affichage.
         * Ne change pas l'état et n'émet pas toggled(bool).
         */
        void setStateIcon(bool state, const QIcon &icon);

        /** @brief Définit l'infobulle d'un état et met à jours le bouton.
         * @param state false pour l'état non coché, true pour l'état coché.
         * @param tip Infobulle affichée pour cet état.
         * Ne change pas l'état et n'émet pas toggled(bool).
         */
        void setStateToolTip(bool state, const QString &tip);

        /// Retourne l'état coché natif du bouton, sans le modifier.
        bool state() const { return isChecked(); }

    public slots:
        /** @brief Applique l'état Qt et synchronise le texte, l'icône et le style.
         * @param checked Nouvel état coché.
         * Masque le slot non virtuel hérité de QAbstractButton. toggled(bool)
         * n'est émis que si l'état change et si les signaux sont actifs.
         * La présentation est actualisée même lorsque les signaux sont bloqués.
         */
        void setChecked(bool checked);

    protected:
        /// Fait passer les activations souris, clavier et click() par notre slot.
        void nextCheckState() override;
        /// Synchronise aussi les appels natifs à setChecked() via QPushButton*.
        void checkStateSet() override;
private:
        struct State { QString text; QIcon icon; QString tip; };
        std::array<State, 2> states_;
        /// Applique la présentation courante et la propriété QSS running.
        void updatePresentation();
        /// Mesure les deux paires texte/icône et réserve leur largeur maximale.
        void updateWidth();
};
