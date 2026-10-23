/**
 * @file DebugPage.h
 * @brief Page de diagnostic des événements et des mesures série.
 */
#pragma once
#include "core/EventQueue.h"
#include "gui/widgets/Page.h"
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <array>
class QTextEdit;
class QPushButton;
class QLineEdit;
class QVBoxLayout;
class QHBoxLayout;

/**
 * @brief Affiche les journaux de diagnostic et permet l'envoi de commandes brutes.
 *
 * Deux vues en lecture seule présentent les événements et les mesures, avec un
 * défilement automatique optionnel. MainWindow transmet uniquement les nouveaux lots.
 *
 * Les styles préchargés par AppSettings sont sélectionnés par Event::type. Les formats de
 * paragraphe et le retrait sont communs aux deux vues. Les commandes saisies sont
 * transmises par le signal hérité sendRawCommand lorsque SystemState l'autorise.
 */

class DebugPage final : public Page {
    Q_OBJECT

    public:
        explicit DebugPage(QWidget *parent = nullptr);
        void triggerPrimaryAction();

    public slots:
        void setVisible(bool visible) override;
        void setSystemState(const SystemState &state) override; // Mémorise l'état et active la saisie et l'envoi selon canSendCommands().
        void processEvents(const EventQueue::Batch &batch) override;
        void processMeasurements(const EventQueue::Batch &batch) override;
        void clearHistory(); // Vide les deux vues.

    private:
        QPushButton *send_, *autoscroll_, *clear_;
        QLineEdit *input_;
        QTextEdit *events_, *measures_;

        struct EventFormat {
            QTextBlockFormat block;
            QTextCharFormat characters;
            QString message = "$";
            QString prefix;
        };
        std::array<EventFormat, 128> formats_; // Formats et modèles indexés par Event::type ; case 0 hors plage.

        // Ajoute les événements ou mesures d'un lot à la vue correspondante.
        void append(QTextEdit *view, const EventQueue::Batch &batch);
        void checkAndSendCommand();
        void makeBlockFormats();
        static QString formatValue(const QString &pattern, const QStringList &values); // Substitution séquentielle : $, ${zéro|autre}, $x.y.
        void clear();

        // Fonctions de construction de l'interface, appelées par le constructeur.
        QVBoxLayout* makeUI();
        QHBoxLayout* makeControlsUI();
        QWidget* makeEventView(QTextEdit *view, const QString &title);
};
