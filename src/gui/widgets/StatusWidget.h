#pragma once

#include "gui/components/StatusIndicator.h"
#include <QWidget>
#include <QString>

class QLabel;

/**
 * @brief Message de statut et voyant, avec priorité à la dernière erreur.
 *
 * Une erreur et un avertissement sont mémorisés indépendamment. Effacer
 * l'erreur révèle l'avertissement éventuel ; sans message, le voyant est vert.
 * Le message reprend la couleur du voyant, définie dans le QSS de StatusIndicator.
 */
class StatusWidget final : public QWidget {
    Q_OBJECT

    public:
        explicit StatusWidget(QWidget *parent = nullptr);
        QString lastError() const { return last_error; }
        QString lastWarning() const { return last_warning; }

    public slots:
        void setWarning(const QString &text); // Remplace l'avertissement mémorisé, sans masquer une erreur présente.
        void setError(const QString &text); // Remplace l'erreur mémorisée et l'affiche en priorité.
        void resetError();
        void resetWarning();
        void reset(); // Efface les deux messages et affiche un voyant vert.
        void setUnknown(); // Efface les messages et affiche un voyant gris pour un état carte inconnu.

    private:
        void updateDisplay();
        StatusIndicator::State currentState() const;
        QString last_error;
        QString last_warning;
        QLabel *message_;
        StatusIndicator *indicator_;
};
