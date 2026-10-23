#pragma once
/** @file
 * @brief Styles et modèles de texte du terminal Debug, définis dans debug.json.
 */
#include <QHash>
#include <QJsonObject>
#include <QTextCharFormat>

/**
 * @brief Présentation d'un type d'événement, indépendante de ses données.
 * Les valeurs par défaut donnent un texte aligné à gauche, sans préfixe ni
 * attribut de caractère imposé. La vue calcule les retraits et substitue les
 * paramètres du modèle ; cette structure ne réalise aucun rendu.
 */
struct DebugStyle {
    QTextCharFormat characters; // Couleurs, soulignement et texte barré explicitement configurés.
    Qt::Alignment alignment = Qt::AlignLeft; // Alignement du paragraphe associé au champ JSON justify.
    QString prefix; // Préfixe vide ou constitué d'un seul caractère Unicode imprimable.
    QString message = "$"; // Modèle de substitution utilisé par DebugPage ; "$" affiche tous les champs.
};

/**
 * @brief Dictionnaire des styles, indexé par caractère de type d'événement.
 *
 * AppSettings charge et valide ce dictionnaire au démarrage, puis le partage
 * en lecture seule avec les vues Debug. La classe traite un objet JSON déjà
 * lu ; elle n'ouvre pas elle-même le fichier debug.json.
 */
class DebugStyles final {
    public:
        /**
         * @brief Valide la liste JSON types et construit les formats correspondants.
         * Chaque type doit être un caractère ASCII imprimable, non blanc et unique.
         * Les champs optionnels absents ou null gardent les valeurs par défaut :
         * - foreground/background : couleurs Qt ; #RRGGBBAA place l'alpha en dernier ;
         * - justify : left, right, center ou justify ;
         * - underline/overstrike : booléens ;
         * - prefix : chaîne vide ou un caractère imprimable ;
         * - message : modèle de texte, "$" par défaut, dont la syntaxe est interprétée par la vue.
         *
         * Les styles existants sont remplacés uniquement si toute la validation
         * réussit ; un échec les conserve, ainsi que hasPrefixes().
         * @param root Objet racine contenant un tableau types non vide.
         * @param error Vidé à l'entrée ; reçoit un diagnostic préfixé par « debug.json : » en cas d'échec.
         * @return true si tous les styles ont été validés et enregistrés.
         */
        bool load(const QJsonObject &root, QString *error = nullptr);
        
        /**
         * @brief Renvoie le style du type, ou le style par défaut si le type est absent.
         * La référence désigne un stockage interne ; ne pas la conserver au-delà
         * de la durée de vie du dictionnaire ou d'un nouveau chargement réussi.
         */
        const DebugStyle &style(char type) const;
    
        bool hasPrefixes() const { return hasPrefixes_; } // Vrai si au moins un style chargé possède un préfixe non vide.
        
    private:
        QHash<char, DebugStyle> styles_; // Formats validés, indexés par Event::type.
        DebugStyle fallback_; // Présentation neutre pour les types non définis.
        bool hasPrefixes_ = false; // Indique aux vues si une colonne de préfixes est nécessaire.
};
