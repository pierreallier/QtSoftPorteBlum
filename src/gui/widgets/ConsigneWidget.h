/**
 * @file ConsigneWidget.h
 * @brief Paramétrage des profils de consigne et aperçu du signal.
 */
#pragma once

#include "serial/Protocol.h"
#include <QColor>
#include <QGroupBox>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QPointF>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class ConsignePreview;
class QTableWidget;
class QPushButton;

/**
 * @brief Édite une consigne échelon, rampe, trapèze, sinus ou tableau de points.
 *
 * Tous les champs sont créés une fois ; le profil sélectionné détermine leur visibilité.
 * Le délai de départ concerne l'échelon, la rampe et le trapèze. Pour le sinus, il est
 * remplacé par un offset vertical dans l'unité d'amplitude. Les crêtes du sinus restent
 * dans les limites autorisées ; une hausse d'amplitude ajuste l'offset si nécessaire.
 *
 * Le widget transmet les paramètres à ConsignePreview et construit les commandes.
 * Leur envoi et la gestion des confirmations restent à la charge de PilotagePage.
 */
class ConsigneWidget final : public QGroupBox {
    Q_OBJECT
    Q_PROPERTY(QColor previewCurveColor MEMBER previewCurveColor_)
    Q_PROPERTY(QColor previewZeroColor MEMBER previewZeroColor_)
    Q_PROPERTY(qreal previewCurveWidth MEMBER previewCurveWidth_)
    Q_PROPERTY(QColor previewBackgroundColor MEMBER previewBackgroundColor_)
    Q_PROPERTY(QColor previewAxisColor MEMBER previewAxisColor_)
    Q_PROPERTY(qreal previewZeroWidth MEMBER previewZeroWidth_)

public:
    
    explicit ConsigneWidget(QWidget *parent = nullptr); // Crée le formulaire et l'aperçu, avec un profil échelon sélectionné par défaut.
    void setDebugMode(bool enabled); // Rend la consigne Test disponible uniquement en mode Debug.
    void setLimitesAmplitude(double minimum, double maximum, const QString &unit); // Applique les bornes et l'unité à l'amplitude en fonction du type de commande
    QString description() const; // Décrit textuellement le profil et ses paramètres avec la locale courante pour l'enregistrement.
    QVector<Protocol::CommandRequest> getCommand() const; // Retourne une commande avec ses valeurs à deux décimales ; les durées sont en millisecondes.

protected:
    bool eventFilter(QObject *watched, QEvent *event) override; // Désélectionne le tableau lors des clics extérieurs.

private:
    friend class ConsignePreview; // Accès aux propriétés de dessin configurées par la feuille de style.

    // Construction et synchronisation de l'interface.
    void makeUI(); // Assemble la sélection, l'aperçu et le formulaire dans une zone défilante.
    QFormLayout *makeForm(); // Crée tous les champs, conservés lors des changements de profil.
    QWidget *makeGraphique(); // Crée et dimensionne l'aperçu.
    QWidget *makeTableUI(); // Édition des points temps/amplitude, triés par temps unique.
    void addTablePoint();
    void resetValues(); // Restaure les paramètres initiaux et les deux points, sans changer de profil.
    void updateTableLimits(bool invertValues = false);
    QVector<QPointF> tablePoints() const;
    void updateForm(); // Adapte les libellés et la visibilité pour chaque type de commande.
    void updateLimits(); // Borne les durées du trapèze et les crêtes du sinus, puis actualise l'aperçu.
    void updateGraphique(); // Transmet le type et les valeurs effectives à l'aperçu.
    double amplitude() const; // Amplitude signée affichée, utilisée par l'aperçu et les commandes.
    Protocol::CMD typeConsigne() const; // Identifiant du protocole associé au profil sélectionné.

    // Champs du formulaire et aperçu, possédés par la hiérarchie des widgets Qt.
    QFormLayout *form_ = nullptr;
    QComboBox *type_;
    QDoubleSpinBox *amplitude_, *duration_, *delay_, *rise_, *fall_, *frequency_, *offset_;
    QCheckBox *invert_; // Inverse le signe de la valeur affichée et les bornes du champ.
    QCheckBox *tableInvert_ = nullptr; // Commande d'inversion placée avec les actions du tableau.
    QWidget *amplitudeBox_ = nullptr;
    double amplitudeMinimum_ = -255, amplitudeMaximum_ = 255; // Bornes de référence avant inversion.
    QLabel *amplitudeLabel_,  *delayLabel_, *riseLabel_, *fallLabel_, *frequencyLabel_, *offsetLabel_;
    ConsignePreview *preview_;
    QTableWidget *table_ = nullptr;
    QWidget *tablePanel_ = nullptr;
    QWidget *testSpace_ = nullptr;
    QPushButton *removePoint_ = nullptr;
    QPushButton *addPoint_ = nullptr;

    // Style du graphique, exposé aux propriétés Qt ci-dessus.
    QColor previewCurveColor_{"#F75A1B"}, previewZeroColor_{Qt::gray};
    qreal previewCurveWidth_ = 3;
    QColor previewBackgroundColor_{Qt::white}, previewAxisColor_{Qt::black};
    qreal previewZeroWidth_ = 1;
};

/**
 * @brief Calcule et dessine le signal à partir du type de commande et d'une copie des paramètres.
 *
 * La rampe conserve sa valeur finale. Le sinus conserve la valeur calculée à la fin
 * de sa durée, offset compris. L'échelon et le trapèze reviennent à zéro.
 * Le tableau interpole linéairement ses points et maintient la dernière amplitude.
 * Les couleurs et épaisseurs sont lues dans le widget source.
 */
class ConsignePreview final : public QWidget {
public:
    // Paramètres du signal : temps en secondes, fréquence en hertz, valeurs dans l'unité d'amplitude.
    struct Parameters {
        double amplitude = 0; // Amplitude signée.
        double duration = 10; // Instant de fin depuis l'origine du graphique, délai inclus.
        double offset = 0; // Délai de départ ; zéro pour le sinus.
        double rise = 0, fall = 0; // Durées de montée et de descente du trapèze.
        double frequency = 1;
        double sinusOffset = 0; // Décalage vertical du sinus.
        QVector<QPointF> points; // Tableau trié : temps en secondes et amplitude signée.
    };

    explicit ConsignePreview(ConsigneWidget *source);
    void setSignal(Protocol::CMD type, const Parameters &parameters); // Copie les paramètres et demande un nouveau dessin.
    QSize sizeHint() const override; // Taille préférée de l'aperçu.
    QSize minimumSizeHint() const override; // Autorise la réduction de l'aperçu par son layout.

protected:
    void paintEvent(QPaintEvent *event) override; // Dessine le signal en pourcentage de l'amplitude maximale autorisée.

private:
    double valeur(double time) const; // Calcul interne du signal pour paintEvent().
    ConsigneWidget *source_; // Widget propriétaire, utilisé pour le style du graphique.
    Protocol::CMD type_ = Protocol::CMD::ConsigneEchelon;
    Parameters parameters_; // Derniers paramètres transmis par updateGraphique().
};
