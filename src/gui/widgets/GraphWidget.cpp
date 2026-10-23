#include "gui/widgets/GraphWidget.h"
#include "core/AppSettings.h"
#include "gui/components/ColorCheckBox.h"
#include <QCheckBox>
#include <QFile>
#include <QDateTime>
#include <QLocale>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QToolTip>
#include <QToolButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
QString displayNumber(double value, int precision = 2) {
    QString text = QString::number(value, 'f', precision);
    while (text.endsWith('0')) text.chop(1);
    if (text.endsWith('.')) text.chop(1);
    return text == "-0" ? QString("0") : text;
}

const QPointF *nearest(const QVector<QPointF> &points, double time, bool sorted) {
    if (points.isEmpty()) return nullptr;
    if (sorted) {
        auto it = std::lower_bound(points.cbegin(), points.cend(), time, [](const auto &p, double t) { return p.x() < t; });
        if (it == points.cend()) return &points.last();
        if (it == points.cbegin()) return &*it;
        return time - (it - 1)->x() <= it->x() - time ? &*(it - 1) : &*it;
    }
    return &*std::min_element(points.cbegin(), points.cend(), [time](const auto &a, const auto &b) {
        return std::abs(a.x() - time) < std::abs(b.x() - time);
    });
}
const QPointF *previousSample(const QVector<QPointF> &points, double time, bool sorted) {
    if (sorted) {
        const auto it = std::upper_bound(points.cbegin(), points.cend(), time,
            [](double t, const auto &point) { return t < point.x(); });
        return it == points.cbegin() ? nullptr : &*(it - 1);
    }
    const QPointF *previous = nullptr;
    for (const auto &point : points)
        if (point.x() <= time && (!previous || point.x() >= previous->x())) previous = &point;
    return previous;
}
bool supported(const QString &source, const QString &id) {
    if (source == "Mesures")
        return QStringList{"target", "doorAngle", "voltage", "current", "motorAngle", "motorSpeed"}.contains(id);
    if (source != "Acceleration") return false;
    for (const auto &prefix : {QString("acceleration"), QString("velocity"), QString("angles")})
        for (int i = 0; i < 3; ++i) if (id == prefix + QString::number(i)) return true;
    return false;
}
}

GraphCanvas::GraphCanvas(GraphWidget *owner) : QWidget(owner), owner_(owner) {
    setObjectName("graphiqueCanvas"); setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip(tr("Molette : zoom ; Ctrl : temps ; Maj : amplitude. Glisser : déplacer. Double-clic : cadrage automatique."));
    fullScale_ = new QPushButton(this);
    fullScale_->setFocusPolicy(Qt::NoFocus);
    fullScale_->setObjectName("boutonPleineEchelle");
    fullScale_->setIcon(QIcon(":/icons/full-scale.svg"));
    fullScale_->setToolTip(tr("Revenir en pleine échelle"));
    fullScale_->setAccessibleName(tr("Revenir en pleine échelle"));
    connect(fullScale_, &QPushButton::clicked, owner_, &GraphWidget::autoRange);
    help_ = new QToolButton(this);
    help_->setFocusPolicy(Qt::NoFocus);
    help_->setAutoRaise(true);
    help_->setIcon(style()->standardIcon(QStyle::SP_MessageBoxQuestion));
    help_->setObjectName("boutonAideGraphique");
    help_->setAccessibleName(tr("Aide à la navigation dans le graphique"));
    help_->setToolTip(tr("Zoomer / dézoomer : molette de la souris.\n"
                         "Abscisses (temps) uniquement : Ctrl + molette.\n"
                         "Ordonnées (amplitude) uniquement : Maj + molette.\n"
                         "Déplacer : maintenir le bouton gauche et glisser.\n"
                         "Réinitialiser le zoom : double-clic ou bouton pleine échelle."));
    help_->setToolTipDuration(20000);
    connect(help_, &QToolButton::clicked, this, &GraphCanvas::showNavigationHelp);
    help_->hide();
    fullScale_->hide();
}

void GraphCanvas::showNavigationHelp() {
    QToolTip::showText(help_->mapToGlobal(QPoint(0, 0)), help_->toolTip(), help_, QRect(), 20000);
}
void GraphCanvas::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const auto plot = area();
    help_->move(qRound(plot.left()) + 6, qRound(plot.top()) + 6);
    fullScale_->move(qRound(plot.right()) - fullScale_->width() - 6,
                     qRound(plot.bottom()) - fullScale_->height() - 6);
}

QFont GraphCanvas::ordinateFont() const {
    QFont labelFont = font();
    if (labelFont.pixelSize() > 0)
        labelFont.setPixelSize(std::max(1, qRound(labelFont.pixelSize() * 2.0 / 3)));
    else
        labelFont.setPointSizeF(std::max(1.0, labelFont.pointSizeF() * 2.0 / 3));
    return labelFont;
}

QRectF GraphCanvas::area() const {
    const int leftMargin = QFontMetrics(ordinateFont()).height() + 10;
    return QRectF(rect()).adjusted(leftMargin, 1, -1, -48);
}

void GraphCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(rect(), owner_->canvasBackgroundColor_);
    const auto a = area(); const auto &g = *owner_;
    auto map = [&](const QPointF &point) {
        return QPointF(a.left() + (point.x() - g.xMin_) / (g.xMax_ - g.xMin_) * a.width(),
                       a.bottom() - (point.y() - g.yMin_) / (g.yMax_ - g.yMin_) * a.height());
    };
    const double targetStep = (g.yMax_ - g.yMin_) * 100 / 10;
    const double magnitude = std::pow(10.0, std::floor(std::log10(targetStep)));
    const double fraction = targetStep / magnitude;
    const double step = (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 2.5 ? 2.5 : fraction <= 5 ? 5 : 10) * magnitude;
    QVector<double> ticks;
    for (double percent = std::ceil(g.yMin_ * 100 / step) * step;
         percent <= g.yMax_ * 100 + step * 1e-6; percent += step)
        ticks.append(percent);
    p.setPen(owner_->gridColor_);
    for (int i = 0; i <= 10; ++i) {
        const double x = a.left() + i * a.width() / 10;
        p.drawLine(QPointF(x, a.top()), QPointF(x, a.bottom()));
    }
    for (double percent : ticks) {
        const double y = map({0, percent / 100}).y();
        p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));
    }
    p.setPen(owner_->plotBorderColor_); p.drawRect(a);
    for (int i = 0; i <= 5; ++i) {
        const double x = a.left() + i * a.width() / 5;
        const double labelLeft = i == 0 ? a.left() : i == 5 ? a.right() - 64 : x - 32;
        const Qt::Alignment alignment = Qt::AlignVCenter
            | (i == 0 ? Qt::AlignLeft : i == 5 ? Qt::AlignRight : Qt::AlignHCenter);
        p.drawText(QRectF(labelLeft, a.bottom() + 3, 64, 18), alignment,
                   QString::number(g.xMin_ + i * (g.xMax_ - g.xMin_) / 5, 'f', 2));
    }
    p.drawText(QRectF(a.left(), a.bottom() + 21, a.width(), 18), Qt::AlignCenter,
               g.timeName_ + (g.timeUnit_.isEmpty() ? QString() : " (" + g.timeUnit_ + ")"));
    const double zero = map({0, 0}).y();
    if (a.top() <= zero && zero <= a.bottom()) {
        p.drawLine(QPointF(a.left(), zero), QPointF(a.right(), zero));
    }
    p.setClipRect(a);
    for (const auto &c : g.curves_) {
        if (!c.check->isChecked() || !c.check->isEnabled()) continue;
        QPainterPath path; bool first = true; double last = -1;
        for (const auto &point : c.points) {
            const QPointF normalized(point.x(), point.y() / c.divisor);
            if (first || point.x() < last) {
                path.moveTo(map(normalized));
            } else {
                path.lineTo(map(normalized));
            }
            first = false; last = point.x();
        }
        const qreal width = c.source == "Acceleration"
            ? owner_->accelerometerCurveWidth_ : owner_->curveWidth_;
        p.setPen(QPen(c.color, width)); p.drawPath(path);
        if (c.points.size() == 1) p.drawEllipse(map({c.points.first().x(), c.points.first().y() / c.divisor}), 2, 2);
    }
    if (g.hovering_) {
        p.setPen(QPen(owner_->cursorColor_, owner_->cursorWidth_, Qt::DashLine));
        const double x = map({g.hoverTime_, 0}).x();
        p.drawLine(QPointF(x, a.top()), QPointF(x, a.bottom()));
    }
    // Graduations en pourcentage de l'échelle propre à chaque courbe.
    p.setClipping(false);
    p.setFont(ordinateFont());
    p.setPen(owner_->axisColor_);
    for (double percent : ticks) {
        const double y = map({0, percent / 100}).y();
        const QString label = displayNumber(percent) + " %";
        const double labelHeight = p.fontMetrics().height();
        const double labelWidth = p.fontMetrics().horizontalAdvance(label);
        const double centerY = std::clamp(y, a.top() + labelWidth / 2, a.bottom() - labelWidth / 2);
        p.save();
        p.translate(a.left() - 6 - labelHeight / 2, centerY);
        p.rotate(-90);
        p.drawText(QRectF(-labelWidth / 2, -labelHeight / 2, labelWidth, labelHeight), Qt::AlignCenter, label);
        p.restore();
    }
}

void GraphCanvas::mouseMoveEvent(QMouseEvent *e) {
    auto &g = *owner_; const auto a = area();
    if (dragging_) {
        const auto delta = e->position() - drag_; drag_ = e->position();
        const double dx = delta.x() / a.width() * (g.xMax_ - g.xMin_);
        g.xMin_ -= dx; g.xMax_ -= dx;
        const double span = g.yMax_ - g.yMin_;
        const double dy = delta.y() / a.height() * span;
        g.yMin_ = std::clamp(g.yMin_ + dy, -1.1, 1.1 - span);
        g.yMax_ = g.yMin_ + span;
    }
    g.hovering_ = a.contains(e->position());
    g.hoverTime_ = g.xMin_ + (e->position().x() - a.left()) / a.width() * (g.xMax_ - g.xMin_);
    // Snap the cursor to the closest sample of a visible curve.
    double distance = std::numeric_limits<double>::infinity(), selected = g.hoverTime_;
    for (const auto &c : g.curves_) if (c.check->isChecked() && c.check->isEnabled()) {
        const auto point = nearest(c.points, g.hoverTime_, c.sorted);
        if (point && std::abs(point->x() - g.hoverTime_) < distance) {
            distance = std::abs(point->x() - g.hoverTime_); selected = point->x();
        }
    }
    g.hoverTime_ = selected; g.showValues(); update();
}

void GraphCanvas::mousePressEvent(QMouseEvent *e) { if (e->button() == Qt::LeftButton) { dragging_ = true; drag_ = e->position(); } }

void GraphCanvas::mouseReleaseEvent(QMouseEvent *) { dragging_ = false; }

void GraphCanvas::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    help_->show();
    fullScale_->show();
}

void GraphCanvas::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    help_->hide();
    fullScale_->hide();
    owner_->hovering_ = false;
    owner_->showValues();
    update();
}

void GraphCanvas::mouseDoubleClickEvent(QMouseEvent *) { owner_->autoRange(); }

void GraphCanvas::wheelEvent(QWheelEvent *e) {
    auto &g = *owner_; const auto a = area();
    const double factor = std::pow(1.0015, -e->angleDelta().y());
    if (!(e->modifiers() & Qt::ShiftModifier)) {
        const double fraction = std::clamp((e->position().x() - a.left()) / a.width(), 0.0, 1.0);
        const double anchor = g.xMin_ + fraction * (g.xMax_ - g.xMin_);
        const double span = std::clamp((g.xMax_ - g.xMin_) * factor, g.timeFactor_, 10000 * g.timeFactor_);
        g.xMin_ = anchor - fraction * span; g.xMax_ = g.xMin_ + span;
    }
    if (!(e->modifiers() & Qt::ControlModifier)) {
        // Without modifiers, keep the point under the mouse fixed on both axes.
        const bool amplitudeOnly = e->modifiers() & Qt::ShiftModifier;
        const double fraction = amplitudeOnly ? 0.5
            : std::clamp((a.bottom() - e->position().y()) / a.height(), 0.0, 1.0);
        const double anchor = amplitudeOnly ? 0.0 : g.yMin_ + fraction * (g.yMax_ - g.yMin_);
        const double span = std::clamp((g.yMax_ - g.yMin_) * factor, 0.01, 2.2);
        g.yMin_ = anchor - fraction * span; g.yMax_ = g.yMin_ + span;
    }
    update(); e->accept();
}



GraphWidget::GraphWidget(QWidget *parent, const QString &configuration) : QWidget(parent) {
    makeUI(configuration);
    reset();
}

void GraphWidget::makeUI(const QString &configuration) {
    setObjectName("graphiqueWidget");
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto frame = new QFrame;
    frame->setObjectName("cadreGraphique");
    outer->addWidget(frame);
    auto layout = new QVBoxLayout(frame);
    layout->setContentsMargins(6, 8, 6, 6);
    canvas_ = new GraphCanvas(this);
    layout->addLayout(makeLegendUI(configuration)); // Widget de sélection des courbes à afficher
    auto content = new QHBoxLayout;
    content->setSpacing(10);
    layout->addLayout(content, 1);
    content->addWidget(canvas_, 1);
    auto side = makeValueUI();
    content->addLayout(side); // Widget d'affichage des valeurs des courbes au point du curseur
    outer->addWidget(makeDebugUI());
    // Export CSV
    auto save = new QPushButton(tr("Enregistrer")); save_ = save; save->setObjectName("boutonSauvegarder"); side->addWidget(save);
    save->setIcon(QIcon(":/icons/save.svg"));
    save->setToolTip(tr("Exporter les données au format csv"));
    connect(save, &QPushButton::clicked, this, &GraphWidget::saveRecording);
}

QWidget *GraphWidget::makeDebugUI() {
    summary_ = new QLabel(tr("Aucune mesure reçue"));
    summary_->setObjectName("measurementSummary");
    summary_->setWordWrap(true);
    summary_->setTextFormat(Qt::PlainText);
    summary_->hide();
    return summary_;
}

QVBoxLayout *GraphWidget::makeValueUI() {
    auto side = new QVBoxLayout;
    auto values = new QFrame; 
    values->setObjectName("cadreValeurs"); 
    auto labels = new QVBoxLayout(values); 
    side->addWidget(values, 1);
    // Titre du panneau de valeurs
    auto title = new QLabel(tr("Valeurs")); 
    title->setObjectName("graphiqueValeursTitre");
    title->setAlignment(Qt::AlignCenter); 
    labels->addWidget(title);
    // Valeurs des courbes
    // Instants
    time_ = new QLabel; 
    time_->setObjectName("graphiqueTemps"); 
    labels->addWidget(time_);
    addValueSeparator(labels);
    // Valeurs des courbes
    for (const auto &source : {QString("Mesures"), QString("Acceleration")}) {
        if (source == "Acceleration") {
            accelSeparator_ = addValueSeparator(labels);
            accelSeparator_->setVisible(accelerometer_->isChecked());
            connect(accelerometer_, &QGroupBox::toggled, accelSeparator_, &QWidget::setVisible);
        }
        for (auto &c : curves_) {
            if (c.source != source) continue;
            c.label = new QLabel;
            c.label->setTextFormat(Qt::RichText);
            labels->addWidget(c.label);
            if (c.source == "Acceleration") {
                c.label->setProperty("class", "valeurInertielle");
                connect(accelerometer_, &QGroupBox::toggled, c.label, &QWidget::setEnabled);
                connect(accelerometer_, &QGroupBox::toggled, c.label, &QWidget::setVisible);
                c.label->setEnabled(accelerometer_->isChecked());
                c.label->setVisible(accelerometer_->isChecked());
            }
        }
    }
    labels->addStretch();
    // Période d'échantillonnage
    addValueSeparator(labels);
    period_ = new QLabel;
    period_->setObjectName("graphiquePeriode");
    period_->setToolTip(tr("Période d'échantillonnage des mesures"));
    period_->setAlignment(Qt::AlignLeft);
    labels->addWidget(period_);
    inertialPeriod_ = new QLabel;
    inertialPeriod_->setObjectName("graphiquePeriodeInertielle");
    inertialPeriod_->setAlignment(Qt::AlignLeft);
    labels->addWidget(inertialPeriod_);
    return side;
}

QWidget *GraphWidget::addValueSeparator(QVBoxLayout *labels) {
    auto line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    labels->addWidget(line);
    return line;
}

QHBoxLayout *GraphWidget::makeLegendUI(const QString &configuration) {
    QHBoxLayout *selection = new QHBoxLayout;
    const auto configurationObject = configuration == ":/courbe.json" ? AppSettings::instance().curves()
                                                                    : AppSettings::readObject(configuration);
    const auto time = configurationObject.value("time").toObject();
    timeName_ = time.value("nom").toString("Temps");
    timeUnit_ = time.value("unite").toString("s");
    const double factor = time.value("facteur").toDouble(0.001);
    if (std::isfinite(factor) && factor > 0) timeFactor_ = factor;
    QJsonArray definitions;
    QHash<QString, QString> inertialTitles;
    for (const auto &entry : configurationObject.value("courbes").toArray()) {
        auto definition = entry.toObject();
        if (definition.value("id").isDouble()) {
            const int index = definition.value("id").toInt();
            const QStringList names = definition.value("source").toString() == "Mesures"
                ? QStringList{"", "voltage", "current", "motorAngle", "motorSpeed", "doorAngle", "target"}
                : QStringList{"", "acceleration", "", "", "velocity", "", "", "angles"};
            definition.insert("id", names.value(index));
        }
        if (definition.value("source").toString() != "Acceleration") {
            definitions.append(definition);
            continue;
        }
        const auto id = definition.value("id").toString();
        const auto colors = definition.value("couleurs").toArray();
        if (!QStringList{"acceleration", "velocity", "angles"}.contains(id) || colors.size() != 3) continue;
        const auto name = definition.value("nom").toString();
        const auto unit = definition.value("unite").toString();
        inertialTitles.insert(id, name + (unit.isEmpty() ? QString() : " (" + unit + ")"));
        for (int axis = 0; axis < 3; ++axis) {
            auto curve = definition;
            curve.insert("id", id + QString::number(axis));
            curve.insert("nom", name + " " + QStringList{"X", "Y", "Z"}.at(axis));
            curve.insert("couleur", colors.at(axis));
            definitions.append(curve);
        }
    }
    for (const auto &source : {QString("Mesures"), QString("Acceleration")}) {
        auto group = new QGroupBox(source == "Mesures" ? tr("Mesures") : tr("Module Inertiel"));
        if (source == "Acceleration") {
            group->setObjectName("centraleInertielle");
            group->setCheckable(true);
            accelerometer_ = group;
            group->setChecked(false);
            connect(group, &QGroupBox::toggled, this, &GraphWidget::accelerometerToggled);
            connect(group, &QGroupBox::toggled, canvas_, qOverload<>(&QWidget::update));
            connect(group, &QGroupBox::toggled, this, &GraphWidget::showValues);
        }
        group->setProperty("class", "panneauCourbes"); auto grid = new QGridLayout(group); selection->addWidget(group, 1);
        // Les marges intérieures sont définies par le padding QSS des cadres.
        grid->setContentsMargins(0, 0, 0, 0);
        if (source == "Mesures") {
            group->setObjectName("mesuresCourbes");
            grid->setVerticalSpacing(8);
            grid->setAlignment(Qt::AlignVCenter);
        }
        QVector<QHBoxLayout *> axes;
        if (source == "Acceleration") {
            const QStringList titles{inertialTitles.value("acceleration"), inertialTitles.value("velocity"), inertialTitles.value("angles")};
            for (int column = 0; column < titles.size(); ++column) {
                if (column > 0) {
                    auto divider = new QFrame;
                    divider->setProperty("class", "separateurVertical");
                    grid->addWidget(divider, 0, column * 2 - 1, 2, 1);
                }
                auto heading = new QToolButton;
                heading->setText(titles[column]);
                heading->setAutoRaise(true);
                heading->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                const QString prefix = QStringList{"acceleration", "velocity", "angles"}.at(column);
                heading->setObjectName("toggleAxes_" + prefix);
                heading->setToolTip(tr("Cocher ou décocher tous les axes du groupe"));
                connect(heading, &QToolButton::clicked, this, [this, prefix] {
                    bool anyChecked = false;
                    for (const auto &curve : curves_)
                        if (curve.source == "Acceleration" && curve.id.startsWith(prefix))
                            anyChecked |= curve.check->isChecked();
                    for (auto &curve : curves_)
                        if (curve.source == "Acceleration" && curve.id.startsWith(prefix))
                            curve.check->setChecked(!anyChecked);
                });
                grid->addWidget(heading, 0, column * 2);
                auto row = new QHBoxLayout; row->setSpacing(4); row->addStretch();
                grid->addLayout(row, 1, column * 2); axes.append(row);
            }
        }
        int index = 0;
        for (const auto &entry : definitions) {
            const auto o = entry.toObject(); if (o.value("source").toString() != source) continue;
            Curve c; c.source = source; c.id = o.value("id").toString(); c.name = o.value("nom").toString();
            if (source == "Mesures") c.dataIndex = QStringList{"", "voltage", "current", "motorAngle", "motorSpeed", "doorAngle", "target"}.indexOf(c.id);
            else c.dataIndex = (c.id.startsWith("acceleration") ? 1 : c.id.startsWith("velocity") ? 4 : 7) + c.id.right(1).toInt();
            c.unit = o.value("unite").toString(); c.color = QColor(o.value("couleur").toString());
            c.scale = o.value("echelle").toDouble(); c.factor = o.value("facteur").toDouble(1);
            if (!supported(source, c.id) || c.name.isEmpty() || !c.color.isValid() || c.scale < 0 || !std::isfinite(c.factor)) continue;
            const QString caption = c.name + (c.unit.isEmpty() ? QString() : " (" + c.unit + ")");
            c.check = source == "Mesures"
                ? static_cast<QCheckBox *>(new ColorCheckBox(caption, c.color))
                : new AxisColorCheckBox(QStringList{"X", "Y", "Z"}.at(c.id.right(1).toInt()), c.color);
            c.check->setObjectName(source + "_" + c.id); c.check->setChecked(o.value("visible").toBool(true));
            if (source == "Mesures") {
                grid->addWidget(c.check, index % 2, index / 2);
            } else {
                const int column = c.id.startsWith("acceleration") ? 0 : (c.id.startsWith("velocity") ? 1 : 2);
                c.check->setToolTip(c.name + (c.unit.isEmpty() ? QString() : " (" + c.unit + ")"));
                c.check->setProperty("axisSelector", true);
                axes[column]->addWidget(c.check);
            }
            ++index;
            connect(c.check, &QCheckBox::toggled, canvas_, qOverload<>(&QWidget::update));
            connect(c.check, &QCheckBox::toggled, this, &GraphWidget::showValues);
            curves_.append(c);
        }
        for (auto row : axes) row->addStretch();
    }
    return selection;
}

void GraphWidget::saveRecording() {
    const auto path = QFileDialog::getSaveFileName(this, tr("Enregistrer les données"), {}, tr("Données CSV (*.csv)"));
    if (!path.isEmpty() && !saveCsv(path)) {
        QMessageBox dialog(QMessageBox::Warning, tr("Enregistrement"), tr("Enregistrement"), QMessageBox::Ok, this);
        dialog.setTextFormat(Qt::PlainText);
        dialog.setInformativeText(tr("Impossible d'enregistrer les donn\u00e9es."));
        dialog.setTextInteractionFlags(Qt::TextSelectableByMouse);
        dialog.exec();
    }
}

void GraphWidget::setVisible(bool visible) {
    if (save_) save_->setShortcut(visible ? QKeySequence("Ctrl+S") : QKeySequence());
    QWidget::setVisible(visible);
}

void GraphWidget::append(const EventQueue::Batch &batch) {
    if (batch.entries.empty() && !batch.lost) return;
    if (!originTimeSet_) {
        for (const auto &entry : batch.entries) {
            if (entry.event->kind() != Event::Kind::Measure) continue;
            originTime_ = static_cast<const MeasureEvent &>(*entry.event).time;
            originTimeSet_ = true;
            break;
        }
    }
    history_.append(batch);
    refresh(history_);
}

void GraphWidget::setOriginTime(int time) {
    if (originTimeSet_ && originTime_ == time) return;
    originTimeSet_ = true;
    originTime_ = time;
    motorSamples_.clear();
    inertialSamples_.clear();
    hovering_ = false;
    refresh(history_);
}

void GraphWidget::setDebugMode(bool enabled) {
    summary_->setVisible(enabled);
}

void GraphWidget::setConsigneUnit(const QString &unit) {
    for (auto &curve : curves_) {
        if (curve.source != "Mesures" || curve.id != "target") continue;
        curve.unit = unit;
        const QString caption = curve.name + (unit.isEmpty() ? QString() : " (" + unit + ")");
        curve.check->setText(caption);
        curve.check->setToolTip(caption);
        showValues();
        return;
    }
}

void GraphWidget::refresh(const MeasurementStore &store) {
    if (!originTimeSet_ && !store.measures.empty()) {
        originTime_ = static_cast<const MeasureEvent &>(*store.measures.front()).time;
        originTimeSet_ = true;
    }
    inertialPeriodText_ = "-- ms";
    double total = 0;
    int count = 0;
    // Utiliser les temps des trames inertielles avant leur association aux mesures moteur.
    for (size_t i = 1; i < store.accelerometer.size(); ++i) {
        const auto previous = static_cast<const AccelerometerEvent &>(*store.accelerometer[i - 1]).time;
        const auto current = static_cast<const AccelerometerEvent &>(*store.accelerometer[i]).time;
        if (current >= previous) {
            total += double(current) - double(previous);
            ++count;
        }
    }
    if (count) inertialPeriodText_ = tr("%1 ms").arg(total * timeFactor_ * 1000 / count, 0, 'f', 0);
    // Update the debug summary
    summary_->setText(tr("Mémoire : %1 mesures ; %2 trames accéléromètre. Perdues avant lecture : %3.")
        .arg(qulonglong(store.measures.size())).arg(qulonglong(store.accelerometer.size())).arg(store.lost));
    
    double latest = 0;
    double earliest = std::numeric_limits<double>::infinity();
    QVector<PlotSample> motor, inertial;
    motor.reserve(qsizetype(store.measures.size()));
    for (const auto &event : store.measures)
        motor.append({event, static_cast<const MeasureEvent &>(*event).time});

    // Recherche du temps de concordance Mesures - Accélération
    QVector<qsizetype> matches(qsizetype(store.accelerometer.size()), -1);
    qsizetype cursor = motor.size();
    qsizetype accelerationIndex = matches.size();
    while (accelerationIndex > 0) { //  Les trames arrivent dans l'ordre temporel : le curseur ne fait que reculer.
        --accelerationIndex;
        const auto time = static_cast<const AccelerometerEvent &>(*store.accelerometer[size_t(accelerationIndex)]).time;
        while (cursor > 0 && motor[cursor - 1].time >= time) --cursor;
        matches[accelerationIndex] = cursor - 1;
    }
    QSet<qsizetype> matchedMotorSamples;
    for (qsizetype i = 0; i < matches.size(); ++i) {
        const auto index = matches[i];
        if (index < 0 || matchedMotorSamples.contains(index)) continue;
        matchedMotorSamples.insert(index);
        inertial.append({store.accelerometer[size_t(i)], motor[index].time});
    }
    // Ajouter les points aux courbes et recalculer les bornes du graphique.
    updateCurvePoints("Mesures", motor, motorSamples_);
    updateCurvePoints("Acceleration", inertial, inertialSamples_);
    for (const auto &c : curves_) {
        for (const auto &point : c.points) earliest = std::min(earliest, point.x());
        if (!c.points.isEmpty()) latest = std::max(latest, c.points.last().x());
    }
    if (std::isfinite(earliest)) {
        xMin_ = std::max(earliest, latest - 10000 * timeFactor_);
        xMax_ = std::max(latest, xMin_ + timeFactor_);
    } else {
        xMin_ = 0;
        xMax_ = 10000 * timeFactor_;
    }
    showValues(); canvas_->update();
}

void GraphWidget::updateCurvePoints(const QString &source, const QVector<PlotSample> &samples,
                                   QVector<PlotSample> &previous) {
    // Réutiliser le suffixe encore présent après l'éviction des plus vieux événements.
    qsizetype removed = 0;
    if (!samples.isEmpty()) {
        while (removed < previous.size() && previous[removed].event != samples.first().event) ++removed;
    } else {
        removed = previous.size();
    }
    qsizetype retained = 0;
    while (retained < samples.size() && removed + retained < previous.size()
           && samples[retained].event == previous[removed + retained].event
           && samples[retained].time == previous[removed + retained].time) ++retained;

    for (auto &curve : curves_) {
        if (curve.source != source) continue;
        if (removed > 0) curve.points.remove(0, std::min(removed, curve.points.size()));
        curve.points.resize(retained);
        curve.points.reserve(samples.size());
        for (qsizetype i = retained; i < samples.size(); ++i) {
            const auto &sample = samples[i];
            const auto value = source == "Mesures"
                ? static_cast<const MeasureEvent &>(*sample.event).rawValues()[curve.dataIndex]
                : static_cast<const AccelerometerEvent &>(*sample.event).rawValues()[curve.dataIndex];
            curve.points.append({(sample.time - originTime_) * timeFactor_, value * curve.factor});
        }
        double peak = 0;
        for (const auto &point : curve.points) peak = std::max(peak, std::abs(point.y()));
        curve.divisor = curve.scale > 0 ? curve.scale : (peak > 0 ? peak : 1);
        curve.sorted = std::is_sorted(curve.points.cbegin(), curve.points.cend(),
            [](const auto &a, const auto &b) { return a.x() < b.x(); });
    }
    previous = samples;
}

void GraphWidget::showValues() {
    // Afficher les valeurs des courbes au point du curseur, ou la dernière valeur si le curseur n'est pas sur le graphique.
    double latest = 0; bool any = false;
    for (const auto &c : curves_) if (!c.points.isEmpty()) { latest = std::max(latest, c.points.last().x()); any = true; }
    if (save_) save_->setEnabled(any);
    // Afficher l'instant du curseur
    time_->setText(timeName_ + " : " + (any ? QString::number(hovering_ ? hoverTime_ : latest, 'f', 3) : "--")
                   + (timeUnit_.isEmpty() ? QString() : " " + timeUnit_));
    QString period = "-- ms";
    // Calculer la période d'échantillonnage des mesures, si disponible.
    for (const auto &c : curves_) if (c.source == "Mesures" && c.points.size() > 1) {
            double total = 0; int count = 0;
            for (qsizetype i = 1; i < c.points.size(); ++i) {
                const double dt = c.points[i].x() - c.points[i - 1].x();
                if (dt >= 0) { total += dt; ++count; }
            }
            if (count) period = tr("%1 ms").arg(total * 1000 / count, 0, 'f', 0);
            break;
    }
    period_->setText(tr("Période d'acquisition : %1").arg(period));
    inertialPeriod_->setText(tr("Période module inertiel : %1").arg(inertialPeriodText_));
    inertialPeriod_->setVisible(accelerometer_->isChecked());
    // Afficher les valeurs des courbes
    for (auto &c : curves_) {
        auto point = hovering_ ? nearest(c.points, hoverTime_, c.sorted) : (c.points.isEmpty() ? nullptr : &c.points.last());
        if (c.source == "Acceleration") {
            if (hovering_ && accelerometer_->isChecked())
                point = previousSample(c.points, hoverTime_, c.sorted);
            else if (hovering_ && point && point->x() != hoverTime_)
                point = nullptr;
        }
        const QString value = (point ? displayNumber(point->y()) : "--")
            + (c.unit.isEmpty() ? QString() : " " + c.unit);
        c.label->setText(QString("<span style=\"color:%1\">%2 : </span>"
                                "<span style=\"color:#000000\">%3</span>")
            .arg(c.color.name(), c.name.toHtmlEscaped(), value.toHtmlEscaped()));
    }
}

void GraphWidget::setAccelerometerEnabled(bool on) {
    const QSignalBlocker blocker(accelerometer_);
    accelerometer_->setChecked(on);
    if (accelSeparator_) accelSeparator_->setVisible(on);
    for (const auto &curve : curves_)
        if (curve.source == "Acceleration") {
            curve.label->setEnabled(on);
            curve.label->setVisible(on);
        }
    showValues();
    canvas_->update();
}

void GraphWidget::setAccelerometerControlEnabled(bool enabled) {
    accelerometer_->setEnabled(enabled);
    canvas_->update();
}

void GraphWidget::reset() {
    summary_->setText(tr("Aucune mesure reçue"));
    history_ = {};
    inertialPeriodText_ = "-- ms";
    motorSamples_.clear();
    inertialSamples_.clear();
    originTime_ = 0;
    originTimeSet_ = false;
    metadata_.clear();
    recordedAt_ = QDateTime::currentDateTime().toString(Qt::ISODate);
    for (auto &c : curves_) c.points.clear();
    hovering_ = false; xMin_ = 0; xMax_ = 10000 * timeFactor_; yMin_ = -1.1; yMax_ = 1.1;
    showValues(); canvas_->update();
}

void GraphWidget::autoRange() {
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (const auto &c : curves_) if (c.check->isChecked() && c.check->isEnabled()) for (const auto &p : c.points) { lo = std::min(lo, p.x()); hi = std::max(hi, p.x()); }
    if (std::isfinite(lo)) {
        xMax_ = hi > lo ? hi : lo + timeFactor_;
        xMin_ = std::max(lo, xMax_ - 10000 * timeFactor_);
    }
    yMin_ = -1.1; yMax_ = 1.1; canvas_->update();
}



bool GraphWidget::saveCsv(const QString &path) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    const QChar separator = locale.decimalPoint() == "," ? ';' : ',';
    auto quote = [](QString text) { text.replace('"', "\"\""); return '"' + text + '"'; };
    out << "# Blum Mesures - " << BLUM_VERSION << " - " << recordedAt_ << '\n';
    for (const auto &line : metadata_) out << "# " << line << '\n';
    const bool inertial = accelerometer_->isChecked() || std::any_of(curves_.cbegin(), curves_.cend(), [](const Curve &c) {
        return c.source == "Acceleration" && !c.points.isEmpty();
    });
    QStringList headers{quote(timeName_ + " [" + timeUnit_ + "]")};
    QVector<const Curve *> selected;

    for (const auto &c : curves_) if (c.source == "Mesures") {
        selected.append(&c); headers << quote(c.name + " [" + c.unit + "]");
    }
    if (inertial) {

        for (const auto &c : curves_) if (c.source == "Acceleration") {
            selected.append(&c); headers << quote(c.name + " [" + c.unit + "]");
        }
    }
    out << headers.join(separator) << '\n';
    QVector<qsizetype> positions(selected.size(), 0);
    // Merge series on their shared timestamps; missing samples leave empty cells.
    while (true) {
        double time = std::numeric_limits<double>::infinity();
        for (int i = 0; i < selected.size(); ++i)
            if (positions[i] < selected[i]->points.size()) time = std::min(time, selected[i]->points[positions[i]].x());
        if (!std::isfinite(time)) break;
        QStringList fields{locale.toString(time, 'f', timeUnit_ == "ms" ? 0 : timeUnit_ == "s" ? 4 : 2)};
        for (int i = 0; i < selected.size(); ++i) {
            const auto &points = selected[i]->points;
            if (positions[i] < points.size() && points[positions[i]].x() == time)
                fields << locale.toString(points[positions[i]++].y(), 'f', 2);
            else fields << QString();
        }
        out << fields.join(separator) << '\n';
    }
    out.flush();
    return out.status() == QTextStream::Ok && file.commit();
}

/* GraphFileWidget - Graphique à partir d'un fichier de mesure précédemment enregistré */
GraphFileWidget::GraphFileWidget(QWidget *parent, const QString &configuration) : GraphWidget(parent, configuration) {
    delete save_;
    save_ = nullptr;
}

bool GraphFileWidget::checkData(const QStringList &lines, QVector<QVector<QPointF>> &points, QString &targetUnit, QString *error, int firstLineNumber) const {
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    if (lines.isEmpty()) return fail(tr("Colonnes de mesures absentes."));
    auto parse = [](const QString &text, QChar separator, bool *ok) {
        QStringList fields; QString field; bool quoted = false;
        for (int i = 0; i < text.size(); ++i) {
            const auto ch = text[i];
            if (ch == '"') {
                if (quoted && i + 1 < text.size() && text[i + 1] == '"') { field += ch; ++i; }
                else quoted = !quoted;
            } else if (ch == separator && !quoted) { fields << field; field.clear(); }
            else field += ch;
        }
        fields << field; *ok = !quoted; return fields;
    };
    bool ok;
    QChar separator = ',';
    auto headers = parse(lines.first(), separator, &ok);
    if (headers.size() == 1) { separator = ';'; headers = parse(lines.first(), separator, &ok); }
    QStringList expected{timeName_ + " [" + timeUnit_ + "]"};

    for (const auto &c : curves_) if (c.source == "Mesures") {
        QString unit = c.unit;
        if (c.id == "target") {
            const auto header = headers.value(expected.size());
            const QString prefix = c.name + " [";
            if (!header.startsWith(prefix) || !header.endsWith(']'))
                return fail(tr("Colonne de consigne invalide."));
            unit = header.mid(prefix.size(), header.size() - prefix.size() - 1);
            if (!unit.isEmpty() && unit != "PWM" && unit != "rad/s" && unit != QString::fromUtf8("°"))
                return fail(tr("Unité de consigne incompatible : %1.").arg(unit));
            targetUnit = unit;
        }
        expected << c.name + " [" + unit + "]";
    }
    const bool inertial = headers.size() > expected.size();
    if (inertial) {

        for (const auto &c : curves_) if (c.source == "Acceleration") expected << c.name + " [" + c.unit + "]";
    }
    if (!ok || headers != expected) return fail(tr("Colonnes de mesures invalides ou incompatibles."));
    points.resize(curves_.size());
    QLocale locale = separator == ';' ? QLocale(QLocale::French) : QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    int lineNumber = firstLineNumber;
    for (qsizetype row = 1; row < lines.size(); ++row) {
        const auto &line = lines[row]; ++lineNumber;
        if (line.trimmed().isEmpty()) continue;
        const auto fields = parse(line, separator, &ok);
        if (!ok || fields.size() != headers.size()) return fail(tr("Ligne %1 invalide.").arg(lineNumber));
        int column = 1;
        for (int i = 0; i < curves_.size(); ++i) {
            const bool a = curves_[i].source == "Acceleration";
            if (a && !inertial) continue;
            const auto &t = fields[0];
            const auto &v = fields[column++];
            if (v.isEmpty()) continue;
            bool timeOk, valueOk;
            double x = locale.toDouble(t, &timeOk), y = locale.toDouble(v, &valueOk);
            if (!timeOk || !valueOk || !std::isfinite(x) || !std::isfinite(y))
                return fail(tr("Valeur numérique invalide à la ligne %1.").arg(lineNumber));
            points[i].append({x, y});
        }
    }
    return true;
}

bool GraphFileWidget::load(const QStringList &lines, QString *error, int firstLineNumber) {
    QVector<QVector<QPointF>> points;
    QString targetUnit;
    if (!checkData(lines, points, targetUnit, error, firstLineNumber)) return false;
    reset();
    if (!targetUnit.isEmpty()) setConsigneUnit(targetUnit);
    for (int i = 0; i < curves_.size(); ++i) {
        auto &c = curves_[i]; c.points = std::move(points[i]);
        double peak = 0;
        for (const auto &p : c.points) peak = std::max(peak, std::abs(p.y()));
        c.sorted = std::is_sorted(c.points.cbegin(), c.points.cend(), [](const auto &a, const auto &b) { return a.x() < b.x(); });
        c.divisor = c.scale > 0 ? c.scale : (peak > 0 ? peak : 1);
    }
    // Affiche les mesures issues de l'accéléromètre uniquement si elles sont présentes dans le fichier
    const bool hasAcceleration = std::any_of(curves_.cbegin(), curves_.cend(), [](const Curve &curve) {
        return curve.source == "Acceleration" && !curve.points.isEmpty();
    });
    displayAccel(hasAcceleration);
    showValues(); autoRange();
    return true;
}

void GraphFileWidget::displayAccel(bool visible) {
    if (accelSeparator_) accelSeparator_->setVisible(visible);
    setAccelerometerEnabled(visible);
    setAccelerometerControlEnabled(visible);
    for (const auto &curve : curves_)
        if (curve.source == "Acceleration") curve.label->setVisible(visible);
}
