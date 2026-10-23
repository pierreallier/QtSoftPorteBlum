#pragma once
#include "core/MeasurementStore.h"
#include <QWidget>
#include <QColor>
#include <QVector>
#include <QPointF>
#include <QStringList>
class QCheckBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;
class GraphWidget;

class GraphCanvas final : public QWidget {

    public:
        explicit GraphCanvas(GraphWidget *owner);

    private:
        void resizeEvent(QResizeEvent *event) override;
        QFont ordinateFont() const;
        QRectF area() const;
        void paintEvent(QPaintEvent *) override; // Gestion du dessin du graphique, des axes et des graduations
        void mouseMoveEvent(QMouseEvent *e) override; // Gestion du survol de la souris pour afficher les valeurs
        void mousePressEvent(QMouseEvent *e) override; // Gestion du clic pour le drag de la vue
        void mouseReleaseEvent(QMouseEvent *) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *) override; // Gestion du double-clic pour réinitialiser l'échelle
        void wheelEvent(QWheelEvent *e) override; // Gestion du zoom par la molette de la souris
        void showNavigationHelp();
        GraphWidget *owner_;
        QPushButton *fullScale_;
        QToolButton *help_;
        QPointF drag_;
        bool dragging_ = false;
};

class GraphWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor gridColor MEMBER gridColor_)
    Q_PROPERTY(QColor axisColor MEMBER axisColor_)
    Q_PROPERTY(QColor cursorColor MEMBER cursorColor_)
    Q_PROPERTY(qreal curveWidth MEMBER curveWidth_)
    Q_PROPERTY(qreal accelerometerCurveWidth MEMBER accelerometerCurveWidth_)
    Q_PROPERTY(QColor canvasBackgroundColor MEMBER canvasBackgroundColor_)
    Q_PROPERTY(QColor plotBorderColor MEMBER plotBorderColor_)
    Q_PROPERTY(qreal cursorWidth MEMBER cursorWidth_)

    public:
        explicit GraphWidget(QWidget *parent = nullptr, const QString &configuration = ":/courbe.json");
        void refresh(const MeasurementStore &store);
        void append(const EventQueue::Batch &batch);
        const MeasurementStore &history() const { return history_; }
        void reset();
        void setOriginTime(int time); // Origine en millisecondes ; les mesures brutes restent inchangées.
        void setDebugMode(bool enabled);
        void setConsigneUnit(const QString &unit); // Change uniquement l'unité ; conserve le facteur de courbe.json.

        // Fonctions pour l'export CSV
        bool saveCsv(const QString &path) const; // Sauvegarde les données. Retourne false si impossible.
        void setRecordingMetadata(const QStringList &lines) { metadata_ = lines; } // Définit l'en-tête du fichier CSV. Les lignes doivent être terminées par '\n'.
        QStringList recordingMetadata() const { return metadata_; } // Retourne l'en-tête du fichier CSV.
        QString recordedAt() const { return recordedAt_; } // Retourne la date et l'heure de l'enregistrement, ou une chaîne vide si inconnue.
        void setRecordedAt(const QString &date) { recordedAt_ = date; } // Définit la date et l'heure de l'enregistrement, pour l'en-tête du fichier CSV.

    public slots:
        void setVisible(bool visible) override;
        void autoRange();
        void setAccelerometerEnabled(bool on);
        void setAccelerometerControlEnabled(bool enabled);

    signals:
        void accelerometerToggled(bool on);

    private:
        friend class GraphCanvas;
        QColor gridColor_{"#DDDDDD"}, axisColor_{"#6B7280"}, cursorColor_{Qt::black};
        qreal curveWidth_ = 2;
        qreal accelerometerCurveWidth_ = 1;
        QColor canvasBackgroundColor_{Qt::white}, plotBorderColor_{Qt::black};
        qreal cursorWidth_ = 1;

    protected:
        struct Curve {
            QString source, id, name, unit;
            int dataIndex = 0;
            bool sorted = true;
            QColor color;
            double scale = 0, factor = 1, divisor = 1;
            QCheckBox *check = nullptr;
            QLabel *label = nullptr;
            QVector<QPointF> points;
        };
        QVector<Curve> curves_;
        MeasurementStore history_;
        QStringList metadata_;
        QString recordedAt_;
        QString timeName_ = "Temps", timeUnit_ = "s";
        double timeFactor_ = 0.001;
        qint64 originTime_ = 0;
        bool originTimeSet_ = false;
        QPushButton *save_;
        QWidget *accelSeparator_ = nullptr;

        void showValues();

    private:
        // Gestion de l'interface utilisateur
        void makeUI(const QString &configuration); // Création de l'interface utilisateur
        QHBoxLayout *makeLegendUI(const QString &configuration); 
        QWidget *makeDebugUI(); // Création du widget d'affichage des informations de débogage
        QVBoxLayout *makeValueUI(); // Création du widget d'affichage des valeurs des courbes au point du curseur
        QWidget *addValueSeparator(QVBoxLayout *labels);
        void saveRecording();

        struct PlotSample {
            EventPtr event;
            qint64 time;
        };
        QVector<PlotSample> motorSamples_, inertialSamples_;
        void updateCurvePoints(const QString &source, const QVector<PlotSample> &samples,
                               QVector<PlotSample> &previous);
        GraphCanvas *canvas_;
        QLabel *time_, *period_, *inertialPeriod_;
        QString inertialPeriodText_ = "-- ms";
        QLabel *summary_;
        QGroupBox *accelerometer_ = nullptr;
        double xMin_ = 0, xMax_ = 10, yMin_ = -1.1, yMax_ = 1.1;
        bool hovering_ = false;
        double hoverTime_ = 0;

};

// Graphique de consultation des fichiers CSV, sans bouton de sauvegarde.
class GraphFileWidget final : public GraphWidget {
    Q_OBJECT

    public:
        explicit GraphFileWidget(QWidget *parent = nullptr, const QString &configuration = ":/courbe.json");
        // La première ligne contient les colonnes ; les suivantes contiennent les valeurs.
        bool load(const QStringList &lines, QString *error = nullptr, int firstLineNumber = 1);
    
        private:
        bool checkData(const QStringList &lines, QVector<QVector<QPointF>> &points, QString &targetUnit, QString *error, int firstLineNumber) const;
        void displayAccel(bool visible);
};
