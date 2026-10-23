#pragma once
/** @file
 * @brief Fenêtre de visualisation des enregistrements CSV de mesures.
 */
#include <QWidget>
#include <QStringList>

class GraphFileWidget;

/**
 * @brief Affiche un enregistrement dans une fenêtre indépendante de l'acquisition.
 *
 * Vérifie l'en-tête, la version et les métadonnées du fichier, puis délègue
 * la lecture des colonnes et l'affichage des courbes à GraphFileWidget.
 * L'en-tête visuel présente le fichier, sa date et les paramètres enregistrés.
 * La fenêtre possède l'attribut WA_DeleteOnClose : sa fermeture détruit l'objet.
 */
class VisuPage final : public QWidget {
    Q_OBJECT

    public:
        /// Construit une fenêtre Qt::Window ; parent conserve les règles de propriété Qt.
        explicit VisuPage(QWidget *parent = nullptr);
        /**
         * @brief Charge un fichier CSV compatible et actualise les courbes et l'en-tête.
         * Accepte uniquement une version normalisée égale à celle de l'application,
         * une date ISO valide et des métadonnées compatibles avec le format actuel.
         * @param path Chemin du fichier de mesures à lire.
         * @param error Reçoit un diagnostic en cas d'échec.
         * @return true si la validation et le chargement des courbes ont réussi.
         */
        bool load(const QString &path, QString *error = nullptr);

    private:
        /// Données extraites et validées avant transmission au graphique.
        struct FileData {
            QStringList lines; // En-tête des colonnes suivi des lignes de mesures.
            QStringList metadata; // Métadonnées de pilotage, sans le préfixe CSV « # » et son espace.
            QString date; // Date ISO issue de la première ligne du fichier.
        };
        
        // Lit et valide le fichier ; renseigne data en cas de succès, error en cas d'échec.
        bool checkFile(const QString &path, FileData &data, QString *error);
        // Reconstruit le panneau d'informations à partir du chemin et des métadonnées du graphique.
        void makeEntete(const QString &path);

        GraphFileWidget *graph_; // Graphique du fichier, possédé par la hiérarchie de widgets de la fenêtre.
        QWidget *information_ = nullptr; // Panneau d'en-tête, remplacé après chaque chargement réussi.
};
