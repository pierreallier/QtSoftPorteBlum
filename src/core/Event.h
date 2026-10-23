#pragma once
/**
 * @file
 * @brief Événements du protocole série et commandes conservés dans les historiques.
 *
 * Les producteurs remplissent les champs avant publication. Les consommateurs
 * partagent ensuite des EventPtr en lecture seule. Les mesures restent des
 * entiers bruts : les conversions physiques et la présentation relèvent des vues.
 */
#include <QByteArray>
#include <QString>
#include <QHash>
#include <QVector>
#include <array>
#include <memory>

/** @brief Base polymorphe d'un événement, à ne plus modifier après publication. */
struct Event {
    // Famille fonctionnelle, indépendante du caractère de protocole `type`.
    enum class Kind { Message, Request, Measure, Accelerometer, Text };
    // Type de protocole : E/W/I/S/R/O/N, M/A, ou Q (commande locale), T (texte).
    char type = 'T';
    virtual ~Event() = default;
    // Identifie la famille concrète ; les états, erreurs et avertissements sont Message.
    virtual Kind kind() const = 0;
    // Contenu sans préfixe de présentation ni libellé de catégorie.
    virtual QString content() const = 0;
    // Octets d'origine, incluant le marqueur et le type pour une trame binaire reçue.
    QByteArray raw;
};

// Propriété partagée en lecture seule pour la publication et les historiques.
using EventPtr = std::shared_ptr<const Event>;

/**
 * @brief Message codé du contrôleur : code sur 8 bits et valeur signée sur 32 bits.
 * Le contenu est résolu par le CodeBook de AppSettings.
 * Le décodeur conserve dans `type` le caractère reçu, y compris R/O/N.
 */
struct MessageEvent : Event {
    MessageEvent() { type = 'I'; }
    quint8 code = 0; qint32 value = 0;
    Kind kind() const override { return Kind::Message; }
    QString content() const override;
};
/**
 * @brief État d'une machine (S), dont `value` est le code d'état.
 * Le numéro d'état est unique parmi toutes les machines. Le libellé est lu
 * dans AppSettings à cet indice, indépendamment de `code`.
 * Les indices non définis et hors limites donnent « État inconnu ».
 */
struct StateEvent final : MessageEvent {
    StateEvent() { type = 'S'; }
    QString content() const override;
};

/// Message d'erreur (E), avec le même décodage de libellé que MessageEvent.
struct ErrorEvent final : MessageEvent {
    ErrorEvent() { type = 'E'; }
};

/// Message d'avertissement (W), avec le même décodage de libellé que MessageEvent.
struct WarningEvent final : MessageEvent {
    WarningEvent() { type = 'W'; }
};

/// Commande locale (Q) : content() renvoie la commande telle que stockée.
struct RequestEvent final : Event {
    RequestEvent() { type = 'Q'; }
    QString command; ///< Texte de la commande envoyée au contrôleur.
    QString content() const override { return command; }
    Kind kind() const override { return Kind::Request; }
};

/// Texte hors trame binaire (T), décodé en UTF-8 et nettoyé par SerialDecoder.
struct TextEvent final : Event {
    QString line; ///< Texte décodé ; les octets originaux restent dans raw.
    Kind kind() const override { return Kind::Text; }
    QString content() const override { return line; }
};

/**
 * @brief Mesure moteur/porte (M), conservée sans conversion d'unité.
 * content() joint les entiers de rawValues() avec des espaces.
 */
struct MeasureEvent final : Event {
    MeasureEvent() { type = 'M'; }
    quint32 time = 0; ///< Temps transmis par le contrôleur, utilisé pour l'association des mesures.
    /// Tension et courant bruts.
    qint16 voltage = 0; qint16 current = 0;
    /// Angle et vitesse du moteur bruts.
    qint32 motorAngle = 0, motorSpeed = 0;
    /// Angle de porte et consigne bruts.
    qint32 doorAngle = 0; qint32 target = 0;
    /// Ordre protocole : temps, tension, courant, angle moteur, vitesse, angle porte, consigne.
    std::array<qint64, 7> rawValues() const { return {time, voltage, current, motorAngle, motorSpeed, doorAngle, target}; }
    Kind kind() const override { return Kind::Measure; }
    QString content() const override;
};

/**
 * @brief Mesure inertielle (A), conservée sans conversion d'unité.
 * content() joint les entiers de rawValues() avec des espaces.
 */
struct AccelerometerEvent final : Event {
    AccelerometerEvent() { type = 'A'; }
    quint32 time = 0; ///< Temps transmis par le contrôleur, utilisé pour l'association des mesures.
    std::array<qint16, 3> acceleration{}, velocity{}, angles{};
    /// Ordre protocole : temps, 3 accélérations, 3 vitesses, 3 angles.
    std::array<qint64, 10> rawValues() const { return {time, acceleration[0], acceleration[1], acceleration[2], velocity[0], velocity[1], velocity[2], angles[0], angles[1], angles[2]}; }
    Kind kind() const override { return Kind::Accelerometer; }
    QString content() const override;
};

/** @brief Dictionnaire des libellés de messages, indexé par code numérique. */
class CodeBook {
public:
    /**
     * @brief Charge un fichier UTF-8 séparé par des points-virgules.
     * NOM;CODE;TEXT : le nom permet aussi une recherche par id().
     * Le code est compris entre 0 et 255. Les guillemets extérieurs du texte
     * sont retirés et les doubles guillemets échappés sont décodés.
     * Les commentaires commencent par # hors guillemets ; les lignes invalides sont ignorées.
     * Après ouverture réussie, remplace le dictionnaire ; les doublons gardent
     * le dernier libellé. Un échec d'ouverture conserve le dictionnaire existant.
     * @return true si le dictionnaire obtenu contient au moins un code.
     */
    bool load(const QString &path);
    int id(const QString &name) const; // CODE associé à NOM, ou -1 si inconnu.
    /**
     * @brief Résout le libellé et remplace ses paramètres par value.
     * `$` insère l'entier ; `$N.d` applique d décimales (0 à 6) et une largeur N
     * pour la partie entière ; `${zéro|autre}` choisit selon la nullité de value.
     * Chaque paramètre utilise la même valeur. Un code absent produit un texte
     * de repli contenant le code et sa valeur.
     */
    QString format(quint8 code, qint32 value) const;
private:
    QHash<int, QString> texts_; // Libellés chargés, avant substitution des paramètres.
    QHash<QString, int> ids_;
};
