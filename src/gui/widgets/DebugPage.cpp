#include "gui/widgets/DebugPage.h"
#include "core/AppSettings.h"
#include <QRegularExpression>
#include <cmath>
#include "core/DebugStyles.h"
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QScrollBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

DebugPage::DebugPage(QWidget *parent) : Page(parent) {
    setLayout(makeUI());
    setSystemState(systemState());

    connect(send_, &QPushButton::clicked, this, &DebugPage::checkAndSendCommand);
    connect(input_, &QLineEdit::returnPressed, this, &DebugPage::checkAndSendCommand);
    connect(clear_, &QPushButton::clicked, this, &DebugPage::clear);

    ensurePolished();
    events_->ensurePolished();
    measures_->ensurePolished();
    makeBlockFormats();
}

void DebugPage::triggerPrimaryAction() {
    clear_->click();
}

void DebugPage::setVisible(bool visible) {
    autoscroll_->setShortcut(visible ? QKeySequence("Ctrl+Space") : QKeySequence());
    Page::setVisible(visible);
}

QVBoxLayout* DebugPage::makeUI() {
    auto layout = new QVBoxLayout();
    layout->addLayout(makeControlsUI());

    auto splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName("debugSplitter");

    events_ = new QTextEdit; events_->setObjectName("debugEvents");
    events_->setPlaceholderText("Événements et commandes");
    auto eventPanel = makeEventView(events_, "Événements");
    eventPanel->layout()->setContentsMargins(0, 0, 9, 0);
    splitter->addWidget(eventPanel);

    measures_ = new QTextEdit; measures_->setObjectName("debugMeasures");
    measures_->setPlaceholderText("Mesures et accéléromètre");
    auto measurePanel = makeEventView(measures_, "Mesures");
    measurePanel->layout()->setContentsMargins(9, 0, 0, 0);
    splitter->addWidget(measurePanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({300, 600});
    layout->addWidget(splitter);

    return layout;
}

QHBoxLayout* DebugPage::makeControlsUI() {
    auto bar = new QHBoxLayout;

    auto input = new QLineEdit;
    input->setPlaceholderText("Commande série");
    input_ = input;

    auto send = new QPushButton("Envoyer");
    send->setIcon(QIcon(":/icons/send.svg"));
    send_ = send;

    auto clear = new QPushButton("Effacer");
    clear->setIcon(QIcon(":/icons/clear.svg"));
    clear_ = clear;

    autoscroll_ = new QPushButton("Défilement auto");
    autoscroll_->setObjectName("boutonDefilement");
    autoscroll_->setIcon(QIcon(":/icons/scroll.svg"));
    autoscroll_->setCheckable(true);
    autoscroll_->setChecked(true);

    bar->addWidget(input, 1);
    bar->addWidget(send);
    bar->addWidget(clear);
    bar->addWidget(autoscroll_);

    return bar;
}

QWidget* DebugPage::makeEventView(QTextEdit *view, const QString &title) {
    auto panel = new QWidget;

    view->setReadOnly(true);
    view->document()->setMaximumBlockCount(201);

    auto panelLayout = new QVBoxLayout(panel);

    auto titleLabel = new QLabel(title);
    titleLabel->setObjectName("sectionTitle");

    panelLayout->addWidget(titleLabel);
    panelLayout->addWidget(view);
    return panel;
}

QString DebugPage::formatValue(const QString &pattern, const QStringList &values) {
    // Sans modèle explicite, conserver toutes les valeurs brutes.
    if (pattern == "$") return values.join(' ');
    static const QRegularExpression token(R"(\$\{([^|}]+)\|([^}]+)\}|\$([0-9]*)\.([0-6])|\$)");
    QString result;
    qsizetype end = 0, index = 0;
    auto matches = token.globalMatch(pattern);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += pattern.mid(end, match.capturedStart() - end);
        if (index >= values.size()) {
            result += match.captured();
        } else {
            const auto &value = values[index++];
            bool numeric = false;
            const auto integer = value.toLongLong(&numeric);
            if (numeric && !match.captured(1).isEmpty()) {
                result += match.captured(integer == 0 ? 1 : 2);
            } else if (numeric && !match.captured(4).isEmpty()) {
                const int width = match.captured(3).toInt();
                const int decimals = match.captured(4).toInt();
                const auto number = QString::number(integer / std::pow(10.0, decimals), 'f', decimals);
                result += number.rightJustified(width + (decimals > 0 ? decimals + 1 : 0), ' ');
            } else {
                result += value;
            }
        }
        end = match.capturedEnd();
    }
    return result + pattern.mid(end);
}

void DebugPage::makeBlockFormats() {
    const auto *settings = &AppSettings::instance();
    const auto &styles = settings->debugStyles();
    // Both views share the indent calculated from the events view's font.
    const qreal indent = styles.hasPrefixes() ? 2 * QFontMetricsF(events_->font()).horizontalAdvance(' ') : 0;
    for (size_t type = 0; type < formats_.size(); ++type) {
        auto &format = formats_[type];
        const auto &style = styles.style(static_cast<char>(type));
        format.characters = style.characters;
        format.message = style.message;
        if (styles.hasPrefixes()) format.prefix = (style.prefix.isEmpty() ? QStringLiteral(" ") : style.prefix) + ' ';
        format.block.setAlignment(style.alignment);
        format.block.setLeftMargin(indent);
        format.block.setTextIndent(-indent); // Wrapped lines align with the text after the prefix column.
    }
}

void DebugPage::checkAndSendCommand() {
    const auto text = input_->text().trimmed();
    if (!systemState().canSendCommands() || text.isEmpty()) return;
    emit sendRawCommand(text.toUpper());
    input_->clear();
}

void DebugPage::clear() {
    events_->clear();
    measures_->clear();
}

void DebugPage::append(QTextEdit *view, const EventQueue::Batch &batch) {
    if (batch.entries.empty()) return;

    // Save the current scroll position to restore it later if autoscroll is not enabled.
    const auto oldScroll = view->verticalScrollBar()->value();
    auto cursor = view->textCursor(); cursor.movePosition(QTextCursor::End);
    
    for (const auto &entry : batch.entries) {
        const auto &e = entry.event;
        // Determine the block format based on the event type, using a default format if the type exceeds the predefined range.
        const auto type = static_cast<unsigned char>(e->type);
        const auto &format = formats_[type < formats_.size() ? type : 0];
        cursor.setBlockFormat(format.block);
        QStringList fields;
        const auto appendRaw = [&fields](const auto &raw) {
            for (const auto value : raw) fields.append(QString::number(value));
        };
        if (e->kind() == Event::Kind::Measure)
            appendRaw(static_cast<const MeasureEvent &>(*e).rawValues());
        else if (e->kind() == Event::Kind::Accelerometer)
            appendRaw(static_cast<const AccelerometerEvent &>(*e).rawValues());
        else
            fields.append(e->content());
        QString text = format.prefix + formatValue(format.message, fields);
        text.replace("\r\n", "\n").replace('\r', '\n').replace('\n', QChar::LineSeparator);
        // Insert the text with the specified character format, followed by a new block to separate entries.
        cursor.insertText(text, format.characters);
        cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    }
    // Restore the scroll position based on the autoscroll setting.
    if (autoscroll_->isChecked()) view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
    else view->verticalScrollBar()->setValue(oldScroll);
}

void DebugPage::setSystemState(const SystemState &state) {
    Page::setSystemState(state);
    send_->setEnabled(state.canSendCommands());
    input_->setEnabled(state.canSendCommands());
}

void DebugPage::clearHistory() {
    events_->clear(); 
    measures_->clear(); 
}

void DebugPage::processEvents(const EventQueue::Batch &batch) {
    append(events_, batch);
}

void DebugPage::processMeasurements(const EventQueue::Batch &batch) {
    append(measures_, batch);
}
