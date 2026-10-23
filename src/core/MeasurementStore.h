#pragma once
#include "core/EventQueue.h"

/**
 * @brief Deux historiques bornés : mesures moteur/porte et mesures inertielles.
 *
 * Conserve les EventPtr reçus, sans copie des événements ni conversion des
 * valeurs brutes. Les séries suivent l'ordre d'insertion et ne sont pas triées
 * par temps de trame. Cette classe n'a pas de verrou : son propriétaire doit
 * l'utiliser uniquement dans le thread GUI.
 */
class MeasurementStore {
    
    public:
        // Séries moteur/porte (2000 entrées) et accéléromètre (400 entrées).
        std::deque<EventPtr> measures, accelerometer;
        // Cumul des pertes signalées par les lots source, sans les évictions locales.
        quint64 lost = 0;
        
        /**
         * @brief Ajoute les événements Measure et Accelerometer d'un lot.
         * Ignore les autres familles. Chaque série pleine évince son entrée la plus
         * ancienne. Ajoute batch.lost au compteur, même si aucune mesure n'est ajoutée.
         * Le curseur du lot n'est pas mémorisé : le consommateur doit gérer son curseur
         * EventQueue et éviter de soumettre plusieurs fois le même lot.
         * @param batch Lot dont les pointeurs d'événements doivent être non nuls.
         */
        void append(const EventQueue::Batch &batch) {
            lost += batch.lost;
            for (const auto &entry : batch.entries) {
                const bool measure = entry.event->kind() == Event::Kind::Measure;
                if (!measure && entry.event->kind() != Event::Kind::Accelerometer) continue;
                auto &series = measure ? measures : accelerometer;
                const size_t capacity = measure ? 2000 : 400;
                if (series.size() == capacity) series.pop_front();
                series.push_back(entry.event);
            }
        }
};
