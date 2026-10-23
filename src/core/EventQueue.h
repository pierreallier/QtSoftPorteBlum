#pragma once

#include "core/Event.h"
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <deque>
#include <algorithm>

/**
 * @brief Historique borné, protégé par mutex, avec lecture non destructive.
 *
 * Chaque insertion reçoit un numéro de séquence croissant à partir de 1.
 * Lorsque l'historique est plein, l'entrée la plus ancienne est évincée.
 * Chaque consommateur conserve son propre curseur, initialement 0, puis le
 * remplace par Batch::cursor après chaque lecture. Lire ne retire aucune entrée
 * et ne modifie pas la position des autres consommateurs.
 *
 * push() et since() utilisent le même verrou. Les événements publiés sont
 * partagés en lecture seule ; les lots restent utilisables après une éviction.
 */
class EventQueue {
    public:
        /// Événement publié et position dans cet historique.
        struct Entry {
            quint64 sequence; // Numéro d'insertion, indépendant du temps transmis dans la trame.
            EventPtr event; // Propriété partagée de l'événement immuable.
        };
        
        /// Instantané obtenu sous verrou lors d'une lecture.
        struct Batch {
            QVector<Entry> entries; // Entrées encore disponibles, dans l'ordre des séquences.
            quint64 cursor = 0; // Dernière séquence publiée lors de la lecture, même si le lot est vide.
            quint64 lost = 0; // Nombre d'entrées évincées depuis le curseur fourni par ce consommateur.
        };

        /// Fixe la capacité de l'historique ; une capacité nulle est ramenée à 1.
        explicit EventQueue(size_t capacity = 200) : capacity_(std::max(size_t{1}, capacity)) {}
        /**
         * @brief Publie un événement et évince l'entrée la plus ancienne si nécessaire.
         * Le producteur ne doit plus modifier l'objet après publication.
         * @param event Pointeur non nul vers un événement complètement renseigné.
         */
        void push(EventPtr event) {
            QMutexLocker lock(&mutex_);
            if (entries_.size() == capacity_) entries_.pop_front();
            entries_.push_back({++sequence_, std::move(event)});
        }
        
        /**
         * @brief Copie les entrées disponibles dont la séquence est supérieure au curseur.
         * @param cursor Dernière séquence traitée dans cette même file, ou 0 au départ.
         * @return Lot contenant les événements disponibles, le prochain curseur et
         * le nombre d'entrées perdues. Les pertes sont propres à cette lecture,
         * non cumulées par la file. Le curseur d'entrée doit provenir de cette file.
         */
        Batch since(quint64 cursor) const {
            QMutexLocker lock(&mutex_);
            Batch b; b.cursor = sequence_;
            if (!entries_.empty() && cursor < entries_.front().sequence - 1)
                b.lost = entries_.front().sequence - 1 - cursor;
            for (const auto &e : entries_) if (e.sequence > cursor) b.entries.push_back(e);
            return b;
        }

    private:
        const size_t capacity_; // Nombre maximal d'entrées retenues.
        mutable QMutex mutex_; // Protège les entrées et le compteur de séquence.
        std::deque<Entry> entries_; // Entrées de la plus ancienne à la plus récente.
        quint64 sequence_ = 0; // Dernière séquence attribuée ; 0 avant toute insertion.
};

// Deux historiques indépendants, partagés par le worker série et ses consommateurs.
struct SharedQueues {
    EventQueue events{200}; // Messages, états, commandes et texte hors trame binaire.
    EventQueue measures{200}; // Mesures moteur/porte et accéléromètre, dans la même file.
};
