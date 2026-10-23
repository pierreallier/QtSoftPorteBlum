#include "gui/widgets/StateGraphWidget.h"
#include "core/AppSettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetricsF>
#include <QtMath>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QToolTip>
#include <QStyle>

namespace {
QSizeF labelSize(const QFontMetricsF &metrics, const QString &text) {
    const auto lines = text.split('\n');
    qreal width = 0;
    for (const auto &line : lines)
        width = qMax(width, qMax(metrics.horizontalAdvance(line), metrics.boundingRect(line).width()));
    return QSizeF(width, metrics.height() + (lines.size() - 1) * metrics.lineSpacing());
}
}

StateGraphWidget::StateGraphWidget(QWidget *parent, const QString &definitionsPath, int calibrationStateThreshold)
    : QWidget(parent), calibrationStateThreshold_(calibrationStateThreshold) {
    setObjectName("stateGraph");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    stateTimer_.setSingleShot(true);
    stateTimer_.setInterval(100);
    stateTimer_.setTimerType(Qt::PreciseTimer);
    connect(&stateTimer_, &QTimer::timeout, this, &StateGraphWidget::showNextState);
    {
        const auto root = definitionsPath == ":/machines.json" ? AppSettings::instance().states()
                                                           : AppSettings::readObject(definitionsPath);
        machines_ = root.value("machines").toArray();
        defaultMachineCode_ = root.value("default_machine").toInt(
            machines_.isEmpty() ? -1 : machines_.first().toObject().value("code").toInt(-1));
    }
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch(1);
    layout->addWidget(makeDebugUI());
    makeHelpUI();
    setEnabled(false);
    reset();
}

void StateGraphWidget::makeHelpUI() {
    auto help = new QToolButton(this);
    help->setObjectName("boutonAideEtats");
    help->setAutoRaise(true);
    help->setIcon(style()->standardIcon(QStyle::SP_MessageBoxQuestion));
    help->setAccessibleName(tr("Aide aux commandes du graphe d'états"));
    help->setToolTip(tr("Certains états sont cliquables.\n"
                        "Au survol : un état coloré indique qu'il est cliquable.\n"
                        "Cliquez avec le bouton gauche pour envoyer la commande."));
    help->setToolTipDuration(20000);
    help->adjustSize();
    help->move(6, 6);
    connect(help, &QToolButton::clicked, this, [help] {
        QToolTip::showText(help->mapToGlobal(QPoint(0, help->height())),
                           help->toolTip(), help, QRect(), 20000);
    });
}

QWidget *StateGraphWidget::makeDebugUI() {
    debugPanel_ = new QWidget(this);
    auto row = new QHBoxLayout(debugPanel_);
    row->setContentsMargins(0, 0, 0, 0);
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        const int code = machine.value("code").toInt();
        auto label = new QLabel;
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setObjectName(code == 250 ? "productionState" : code == 251 ? "calibrationState" : QString("machineState%1").arg(code));
        debugLabels_.insert(code, label);
        row->addWidget(label, 1);
    }
    debugPanel_->hide();
    return debugPanel_;
}

void StateGraphWidget::setDebugMode(bool enabled) {
    debugPanel_->setVisible(enabled);
    layout()->activate();
    update();
}

void StateGraphWidget::updateDebugStates() {
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        const int code = machine.value("code").toInt();
        debugLabels_.value(code)->setText(machine.value("name").toString() + "\n" + stateDescription(code));
    }
}

int StateGraphWidget::currentMachineCode() const {
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        const auto condition = machine.value("display_when").toObject();
        if (!condition.value("machine").isDouble() || !condition.value("state").isDouble()) continue;
        const auto state = active_.constFind(condition.value("machine").toInt());
        if (state != active_.cend() && state.value() == condition.value("state").toInt())
            return machine.value("code").toInt();
    }
    return defaultMachineCode_;
}

void StateGraphWidget::setState(int state) {
    setState(state < calibrationStateThreshold_ ? 250 : 251, state);
}

void StateGraphWidget::setState(int machineCode, int state) {
    // Repeated notifications must not extend the display queue.
    int previous = active_.value(machineCode, -1);
    for (const auto &pending : pendingStates_)
        if (pending.first == machineCode) previous = pending.second;
    if (previous == state) return;
    pendingStates_.enqueue(qMakePair(machineCode, state));
    if (!stateTimer_.isActive()) showNextState();
}

void StateGraphWidget::showNextState() {
    if (pendingStates_.isEmpty()) return;
    const auto next = pendingStates_.dequeue();
    active_[next.first] = next.second;
    updateDebugStates();
    update();
    stateTimer_.start();
}
void StateGraphWidget::reset() {
    stateTimer_.stop();
    pendingStates_.clear();
    active_.clear();
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        if (machine.value("default_state").isDouble())
            active_.insert(machine.value("code").toInt(), machine.value("default_state").toInt());
    }
    updateDebugStates();
    update();
}
void StateGraphWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::EnabledChange) {
        hovering_ = false;
        unsetCursor();
        update();
    }
    QWidget::changeEvent(event);
}
QList<StateGraphWidget::Node> StateGraphWidget::nodes() const {
    QList<Node> result;
    const auto area = graphArea();
    const qreal h = stateHeight();
    int commonPixelSize = 0;
    auto maxWidthFor = [&](const Node &node) {
        return qMax(1.0, qMin(area.width() * machineRatio("etat_width", 0.8),
            2 * qMin(node.center.x() - area.left(), area.right() - node.center.x())));
    };
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        if (machine.value("code").toInt() != currentMachineCode()) continue;
        for (const auto &value : machine.value("etats").toArray()) {
            const auto object = value.toObject();
            if (object.value("type").toString() == "hidden") continue;
            Node node;
            node.code = object.value("code").toInt();
            node.fullName = object.value("nom").toString();
            node.name = object.value("alt").toString(node.fullName);
            node.type = object.value("type").toString();
            node.command = object.value("cmd").toString();
            node.center = area.topLeft() + QPointF(object.value("x").toDouble() * area.width(), object.value("y").toDouble() * area.height());
            if (node.type == "start" || node.type == "end") node.shape.addEllipse(node.center, stateDiameter() / 2, stateDiameter() / 2);
            else {
                const qreal maxWidth = maxWidthFor(node);
                const qreal padding = qMin(8.0, qMin(h, maxWidth) * 0.1);
                // Reserve 10% of the label width for the active state's bold text.
                node.font = graphFont(node.name, QSizeF(qMax(1.0, (maxWidth - 2 * padding) * 0.9), qMax(1.0, h - 2 * padding)),
                                      machine.value("etat_font_max_px").toInt(0));
                commonPixelSize = commonPixelSize == 0 ? node.font.pixelSize() : qMin(commonPixelSize, node.font.pixelSize());
            }
            result.append(node);
        }
    }
    // All labels share the smallest of their individually calculated maximum sizes.
    for (auto &node : result) {
        if (node.type == "start" || node.type == "end") continue;
        node.font.setPixelSize(qMax(1, commonPixelSize));
        const QFontMetricsF metrics(node.font);
        const qreal maxWidth = maxWidthFor(node);
        const qreal padding = qMin(8.0, qMin(h, maxWidth) * 0.1);
        const auto textSize = labelSize(metrics, node.name);
        const qreal w = qMin(maxWidth, textSize.width() / 0.9 + 2 * padding);
        const qreal actualHeight = qMin(h, textSize.height() + 2 * padding);
        node.shape.addRoundedRect(QRectF(node.center - QPointF(w / 2, actualHeight / 2), QSizeF(w, actualHeight)), actualHeight * 0.15, actualHeight * 0.15);
    }
    return result;
}

QString StateGraphWidget::activeStateName() const {
    return stateName(currentMachineCode());
}

QString StateGraphWidget::stateDescription(int machineCode) const {
    if (!active_.contains(machineCode)) return tr("inconnu");
    return QString("%1 : %2").arg(active_.value(machineCode)).arg(stateName(machineCode));
}

QString StateGraphWidget::stateName(int machineCode) const {
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        if (machine.value("code").toInt() != machineCode) continue;
        for (const auto &value : machine.value("etats").toArray()) {
            const auto state = value.toObject();
            if (state.value("code").toInt() == active_.value(machineCode, -1))
                return state.value("nom").toString();
        }
    }
    return tr("inconnu");
}

qreal StateGraphWidget::stateHeight() const {
    return qMax(1.0, graphArea().height() * machineRatio("etat_height", 0.1));
}

qreal StateGraphWidget::stateDiameter() const {
    return qMax(1.0, graphArea().height() * machineRatio("etat_diameter", 0.05));
}

qreal StateGraphWidget::machineRatio(const QString &key, qreal fallback) const {
    for (const auto &entry : machines_) {
        const auto machine = entry.toObject();
        if (machine.value("code").toInt() == currentMachineCode())
            return qBound(0.001, machine.value(key).toDouble(fallback), 1.0);
    }
    return fallback;
}

QRectF StateGraphWidget::graphArea() const {
    // Coordinates are percentages of the actual space assigned by the layout.
    // Node dimensions must not shrink or translate this coordinate system.
    const auto available = contentsRect();
    const int height = debugPanel_->isHidden() ? available.height()
        : debugPanel_->geometry().top() - available.top() - layout()->spacing();
    return QRectF(available.x(), available.y(), qMax(1, available.width()), qMax(1, height));
}

QFont StateGraphWidget::graphFont(const QString &text, const QSizeF &available, int maxPixelSize) const {
    QFont result = font();
    int low = 1, high = qMax(1, qCeil(available.height()));
    if (maxPixelSize > 0) high = qMin(high, maxPixelSize);
    int best = 1;
    while (low <= high) {
        const int pixels = low + (high - low) / 2;
        result.setPixelSize(pixels);
        const QFontMetricsF metrics(result);
        const auto textSize = labelSize(metrics, text);
        const bool fits = textSize.height() <= available.height()
            && textSize.width() <= available.width();
        if (fits) {
            best = pixels;
            low = pixels + 1;
        } else {
            high = pixels - 1;
        }
    }
    result.setPixelSize(best);
    return result;
}

void StateGraphWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QJsonObject machine;
    for (const auto &entry : machines_) {
        if (entry.toObject().value("code").toInt() == currentMachineCode()) { machine = entry.toObject(); break; }
    }
    const auto list = nodes();
    auto find = [&](int code) -> const Node * {
        for (const auto &node : list) if (node.code == code) return &node;
        return nullptr;
    };
    auto anchor = [](const Node &node, const QPointF &inside, const QPointF &outside) {
        const auto delta = outside - inside;
        qreal low = 0, high = 1;
        for (int i = 0; i < 24; ++i) {
            const auto middle = (low + high) / 2;
            if (node.shape.contains(inside + delta * middle)) low = middle; else high = middle;
        }
        return inside + delta * high;
    };
    painter.setPen(QPen(stateBorderColor_, borderWidth_));
    for (const auto &entry : machine.value("transitions").toArray()) {
        const auto transition = entry.toObject();
        const auto start = find(transition.value("start").toInt());
        const auto end = find(transition.value("end").toInt());
        if (!start || !end) continue;
        QList<QPointF> route;
        QPointF previous = start->center;
        // Offsets are accumulated from the source center before clipping to the shapes.
        for (const auto &value : transition.value("points").toArray()) {
            const auto point = value.toObject();
            previous += QPointF(point.value("x").toDouble() * graphArea().width(), point.value("y").toDouble() * graphArea().height());
            if (route.isEmpty() || QLineF(route.last(), previous).length() > 0.001)
                route.append(previous);
        }
        QPointF sourceInside = start->center;
        QPointF targetInside = end->center;
        // Preserve the last hidden waypoint: clipping must follow the actual segment,
        // rather than replacing it with a ray from the state's center.
        while (!route.isEmpty() && start->shape.contains(route.first())) sourceInside = route.takeFirst();
        while (!route.isEmpty() && end->shape.contains(route.last())) targetInside = route.takeLast();
        const auto first = route.isEmpty() ? targetInside : route.first();
        const auto last = route.isEmpty() ? sourceInside : route.last();
        if (start->shape.contains(first) || end->shape.contains(last)) continue;
        if (QLineF(sourceInside, first).length() < 0.001 || QLineF(targetInside, last).length() < 0.001) continue;
        const auto from = anchor(*start, sourceInside, first);
        const auto to = anchor(*end, targetInside, last);
        QPainterPath path(from);
        for (const auto &point : route) path.lineTo(point);
        const auto tail = path.currentPosition() - to;
        const qreal tailLength = QLineF(QPointF(), tail).length();
        if (tailLength < 0.001) continue;
        const auto arrival = tail / tailLength;
        path.lineTo(to);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        const auto side = QPointF(-arrival.y(), arrival.x());
        painter.setBrush(painter.pen().color());
        painter.drawPolygon(QPolygonF{to, to + arrival * 15 + side * 7, to + arrival * 15 - side * 7});
    }
    for (const auto &node : list) {
        const bool active = node.code == active_.value(currentMachineCode(), -1);
        auto stateFont = node.font;
        if (active) stateFont.setBold(activeTextBold_);
        painter.setFont(stateFont);
        const bool hovered = hovering_ && isEnabled() && !node.command.isEmpty() && node.shape.contains(hoverPosition_);
        const QColor border = hovered ? hoverBorderColor_ : active ? activeBorderColor_ : stateBorderColor_;
        const QColor background = hovered ? hoverBackgroundColor_ : active ? activeBackgroundColor_ : stateBackgroundColor_;
        const QColor ink = active ? activeBorderColor_ : palette().color(QPalette::WindowText);
        painter.setPen(QPen(border, borderWidth_));
        painter.setBrush(node.type == "start" ? border : background);
        painter.drawPath(node.shape);
        painter.setPen(ink);
        if (node.type == "end") { painter.setPen(Qt::NoPen); painter.setBrush(border); painter.drawEllipse(node.center, stateDiameter() * 0.22, stateDiameter() * 0.22); }
        else if (node.type != "start") painter.drawText(node.shape.boundingRect(), Qt::AlignCenter, node.name);
    }
}

QString StateGraphWidget::commandAt(const QPointF &position) const {
    if (!isEnabled()) return {};
    for (const auto &node : nodes()) if (node.shape.contains(position)) return node.command;
    return {};
}
void StateGraphWidget::mouseReleaseEvent(QMouseEvent *event) {
    const auto command = commandAt(event->position());
    if (event->button() == Qt::LeftButton) {
        if (!command.isEmpty()) emit sendRawCommand(command);
    }
    QWidget::mouseReleaseEvent(event);
}
void StateGraphWidget::mouseMoveEvent(QMouseEvent *event) {
    hoverPosition_ = event->position();
    hovering_ = true;
    update();
    const auto command = commandAt(event->position());
    setCursor(command.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    QWidget::mouseMoveEvent(event);
}

void StateGraphWidget::leaveEvent(QEvent *event) {
    hovering_ = false;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}
