#pragma once
/**
 * @file
 * @brief Accès centralisé aux ressources embarquées et aux réglages utilisateur.
 */
#include "core/Event.h"
#include "core/DebugStyles.h"
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include "serial/CommandIds.h"



/**
 * @brief Configuration de l'application, accessible via instance().
 *
 * Les ressources etats.csv, machines.json, codes.csv, courbe.json et debug.json sont chargées
 * une seule fois et partagées en lecture seule entre toutes les instances.
 * Chaque instance désigne un fichier INI, relu à chaque appel à value(). Les clés
 * absentes utilisent les valeurs de la ressource :/config.ini.
 *
 * Appeler initialize() au démarrage pour vérifier les ressources avant de créer
 * les vues. Les accesseurs de ressources déclenchent aussi leur chargement au
 * premier accès, mais ne signalent pas eux-mêmes les erreurs de chargement.
 */
class AppSettings final {

    public:
        static const AppSettings &instance(const QString &path = QString()); // Renvoie l'instance partagée associée au chemin INI résolu en absolu.
        static bool initialize(QString *error = nullptr); // Précharge et valide les ressources embarquées, sans lire le fichier INI.
        
        const QJsonObject &states() const; // JSON normalisé : références CSV et id des transitions résolus en codes série.
        const QJsonObject &curves() const; // Définition JSON des courbes et des conversions de leurs valeurs brutes.
        const CodeBook &codes() const; // Dictionnaire des messages chargé depuis codes.csv.
        const DebugStyles &debugStyles() const; // Retourne les styles de texte du terminal, validés au démarrage et partagés en lecture seule.
        QString stateName(qint32 number) const; // Retourne le nom d'état pour un numéro donné, ou « État inconnu » si le numéro n'est pas défini.
        int stateId(const QString &name) const; // Retourne VALEUR pour le nom Arduino TYPE_ETAT, ou -1 si inconnu.
        int codeId(const QString &name) const; // Retourne CODE pour NOM dans codes.csv, ou -1 si inconnu.
        QString command(const QString &name) const; // Texte de la commande nommée dans protocol.json, ou chaîne vide si inconnue.
        QString command(Protocol::CMD command) const; // Texte associé à l'identifiant de commande généré.
        const QMap<QString, QString> &commands() const; // Dictionnaire partagé des clés et textes de commandes.

        static QJsonObject readObject(const QString &path, QString *error = nullptr); // Lit un fichier JSON
        QVariant value(const QString &key) const; // retourne la valeur associé à la clé (éventuellement modifiée par l'utilisateur)
        static QVariant defaultValue(const QString &key); // Retourne le valeur par défaut associé à la clé
        
        /**
         * @brief Écrit les clés fournies dans le fichier INI et synchronise sur disque.
         * Conserve les autres clés et crée le fichier si nécessaire. Les valeurs
         * sont écrites telles quelles ; leur validation est effectuée lors de la lecture.
         * @param error Reçoit un diagnostic en cas d'échec ; reste inchangé en cas de succès.
         * @return true si QSettings ne signale aucune erreur après synchronisation.
         */
        bool save(const QMap<QString, QVariant> &values, QString *error = nullptr) const;

        QString openFolder() const; // Retourne le répertoire configuré où ouvrir les fichiers.

    private:
        explicit AppSettings(const QString &path); // Construction réservée à instance()
        AppSettings(const AppSettings &) = delete;
        AppSettings &operator=(const AppSettings &) = delete;

        struct Resources; // Données embarquées et partageés
        static const Resources &resources(); // charge les ressources, puis les retourne directement.
        QString path_; // Chemin du fichier INI, fixé à la construction.
};
