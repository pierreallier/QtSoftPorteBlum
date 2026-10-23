#include "gui/widgets/VisualisationPage.h"
#include "gui/widgets/GraphWidget.h"
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QVersionNumber>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

VisuPage::VisuPage(QWidget *parent) : QWidget(parent, Qt::Window) {
    setObjectName("fenetreMesures");
    setAttribute(Qt::WA_DeleteOnClose);
    
    auto layout = new QVBoxLayout(this);
    auto frame = new QFrame;
    frame->setObjectName("cadreVisualisationMesures");
    auto graphLayout = new QVBoxLayout(frame);
    graphLayout->setContentsMargins(10, 10, 10, 10);
    graph_ = new GraphFileWidget(frame);
    graphLayout->addWidget(graph_);
    layout->addWidget(frame, 1);
    resize(1200, 750);
}

bool VisuPage::load(const QString &path, QString *error) {
    FileData data;
    if (!checkFile(path, data, error)) return false;
    if (!graph_->load(data.lines, error, data.metadata.size() + 2)) return false;
    graph_->setRecordingMetadata(data.metadata);
    graph_->setRecordedAt(data.date);
    makeEntete(path);
    setWindowTitle(tr("Mesures \u2014 %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool VisuPage::checkFile(const QString &path, FileData &data, QString *error) {
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return fail(file.errorString());
    QTextStream in(&file);
    QString first = in.readLine();
    if (first.startsWith(QChar(0xfeff))) first.remove(0, 1);
    const QString prefix = "# Blum Mesures - ";
    if (!first.startsWith(prefix)) return fail(tr("Format de fichier de mesures invalide."));
    const auto versionEnd = first.indexOf(" - ", prefix.size());
    if (versionEnd < 0) return fail(tr("En-tête du fichier de mesures invalide."));
    const QString fileVersionText = first.mid(prefix.size(), versionEnd - prefix.size());
    qsizetype suffix = 0;
    const auto fileVersion = QVersionNumber::fromString(fileVersionText, &suffix);
    if (fileVersion.isNull() || suffix != fileVersionText.size())
        return fail(tr("Version du fichier de mesures invalide."));
    const auto currentVersion = QVersionNumber::fromString(BLUM_VERSION);
    const int comparison = QVersionNumber::compare(fileVersion.normalized(), currentVersion.normalized());
    if (comparison < 0)
        return fail(tr("Impossible d'ouvrir ce fichier : sa version %1 est antérieure à la version actuelle %2.")
            .arg(fileVersionText, BLUM_VERSION));
    if (comparison > 0)
        return fail(tr("Version du fichier de mesures incompatible : %1 (version actuelle : %2).")
            .arg(fileVersionText, BLUM_VERSION));
    const QString dateText = first.mid(versionEnd + 3);
    if (!QDateTime::fromString(dateText, Qt::ISODate).isValid()) return fail(tr("Date d'enregistrement invalide."));
    QStringList metadata;
    QString line = in.readLine();
    while (line.startsWith("# ")) { metadata << line.mid(2); line = in.readLine(); }
    if (!metadata.isEmpty()) {
        if (metadata.size() != 2 || !metadata.value(1).startsWith("CONSIGNE ")
            || metadata.first().startsWith("PID "))
            return fail(tr("En-tête de pilotage incompatible : ancien format CSV non pris en charge."));
        if (metadata.first().startsWith(QStringLiteral("Boucle Fermée")) && !metadata.first().contains("(Kp="))
            return fail(tr("En-tête d’asservissement invalide : gains PID manquants."));
    }
    QStringList lines{line};
    while (!in.atEnd()) lines.append(in.readLine());
    if (in.status() != QTextStream::Ok) return fail(tr("Lecture du fichier impossible."));
    data.lines = lines;
    data.metadata = metadata;
    data.date = dateText;
    return true;
}

void VisuPage::makeEntete(const QString &path) {
    delete information_;
    information_ = new QFrame(this);
    information_->setObjectName("cadreInformationsMesures");
    auto information = new QVBoxLayout(information_);
    information->setContentsMargins(12, 10, 12, 10);
    information->setSpacing(4);
    static_cast<QVBoxLayout *>(layout())->insertWidget(0, information_);
    auto summary = new QHBoxLayout;
    summary->setSpacing(16);
    information->addLayout(summary);
    auto makeLabel = [](const QString &title, const QString &value, bool pidIndices = false) {
        QString text = value.toHtmlEscaped();
        if (pidIndices) {
            text.replace("Kp=", "K<sub>p</sub>=");
            text.replace("Ki=", "K<sub>i</sub>=");
            text.replace("Kd=", "K<sub>d</sub>=");
        }
        auto label = new QLabel("<b>" + title.toHtmlEscaped() + " :</b> " + text);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        auto font = label->font(); font.setBold(false); label->setFont(font);
        return label;
    };

    const auto metadata = graph_->recordingMetadata();
    const auto date = QDateTime::fromString(graph_->recordedAt(), Qt::ISODate);
    const auto fileName = QFileInfo(path).fileName();
    const QString dateSeparator = metadata.isEmpty() ? QString(" - ") : QString("<br>");
    auto dateLabel = new QLabel("<b>" + fileName.toHtmlEscaped() + "</b>" + dateSeparator
        + date.toString("dd/MM/yyyy HH:mm:ss").toHtmlEscaped());
    dateLabel->setTextFormat(Qt::RichText);
    dateLabel->setWordWrap(false);
    dateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    summary->addWidget(dateLabel);
    if (!metadata.isEmpty()) {
        auto separator = new QFrame;
        separator->setProperty("class", "separateurVertical");
        summary->addWidget(separator);
        auto pilotage = new QVBoxLayout;
        pilotage->setSpacing(4);
        auto type = makeLabel(tr("Type d\u2019asservissement"), metadata.first(), true);
        type->setWordWrap(false);
        pilotage->addWidget(type);
        pilotage->addWidget(makeLabel(tr("Consigne"), metadata.value(1).mid(QString("CONSIGNE ").size())));
        summary->addLayout(pilotage);
    }
    summary->addStretch();
}
