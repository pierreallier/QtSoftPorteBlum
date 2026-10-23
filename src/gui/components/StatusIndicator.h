#pragma once

#include "gui/components/StyleHelpers.h"
#include <QLabel>
#include <QEvent>
#include <QPainter>

/**
 * @brief Voyant circulaire dont la couleur est définie dans le QSS.
 *
 * La propriété dynamique "state" vaut "error", "warning" ou "ok" selon
 * l'état logique. Elle vaut "unknown" lorsque le widget est désactivé.
 * L'état logique reste mémorisé pendant la désactivation et est rétabli
 * dans cette propriété à la réactivation.
 *
 * Le dessin utilise la couleur de texte de la palette, réglée par exemple par :
 * @code
 * StatusIndicator[state="warning"] { color: #F75A1B; }
 * StatusIndicator:disabled { color: #6B7280; }
 * @endcode
 *
 * La forme est imposée par le code : un cercle centré dans contentsRect(),
 * dont le diamètre est la plus petite dimension disponible. Le texte et le
 * pixmap hérités de QLabel ne sont pas dessinés. setToolTip() reste utilisable.
 */
class StatusIndicator final : public QLabel {
    Q_OBJECT

    public:
        /// États logiques ; leurs couleurs sont personnalisables dans le QSS.
        enum class State {
            Error,   ///< Sélecteur [state="error"], rouge dans le thème fourni.
            Warning, ///< Sélecteur [state="warning"], orange dans le thème fourni.
            Ok       ///< Sélecteur [state="ok"], vert dans le thème fourni.
        };

        /// Crée un voyant de 14 × 14 pixels, initialisé à Error.
        /// La taille peut ensuite être modifiée par setFixedSize().
        explicit StatusIndicator(QWidget *parent = nullptr) : QLabel(parent) {
            setFixedSize(14, 14);
            setState(State::Error);
        }

        /// Renvoie l'état logique mémorisé, même lorsque le voyant est désactivé.
        State state() const { return state_; }

        /**
         * @brief Mémorise l'état et actualise la propriété utilisée par le QSS.
         * setStyleProperty() réévalue les sélecteurs et demande le rafraîchissement.
         * Si le widget est désactivé, la propriété reste "unknown".
         * Utiliser cette méthode plutôt que modifier directement setProperty("state").
         */
        void setState(State state) {
            state_ = state;
            const auto name = !isEnabled() ? QStringLiteral("unknown")
                            : state == State::Ok ? QStringLiteral("ok")
                            : state == State::Warning ? QStringLiteral("warning")
                                                    : QStringLiteral("error");
            setStyleProperty(this, "state", name);
        }

    protected:
        /// Synchronise le style à la désactivation et à la réactivation,
        /// y compris lorsque le changement provient d'un widget parent.
        void changeEvent(QEvent *event) override {
            QLabel::changeEvent(event);
            if (event->type() == QEvent::EnabledChange) setState(state_);
        }

        /// Dessine un disque sans bordure, avec anticrénelage, dans la couleur
        /// WindowText du groupe de palette Active ou Disabled selon isEnabled().
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
            const QRectF area(contentsRect());
            const qreal diameter = qMin(area.width(), area.height());
            QRectF circle(0, 0, diameter, diameter);
            circle.moveCenter(area.center());
            painter.drawEllipse(circle);
        }

    private:
        /// État logique indépendant de la propriété de style "unknown".
        State state_ = State::Error;
};
