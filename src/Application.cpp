#include "Application.h"
#include "gui/StartupSplash.h"
#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include <QColor>
#include <QPalette>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QDebug>

namespace {
    QPalette setFocusPalette(QPalette palette, QColor color) {
        palette.setColor(QPalette::Highlight, color);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        palette.setColor(QPalette::Accent, color);
    #endif
        return palette;
    }
}

Application::Application(int &argc, char **argv) : QApplication(argc, argv) {
    setOrganizationName("PorteBlum");
    setApplicationName("SoftPorteBlum");
    setApplicationVersion(BLUM_VERSION);
#ifdef Q_OS_WIN
    setWindowIcon(QIcon(":/icons/icone.ico"));
#else
    setWindowIcon(QIcon(":/icons/icone.svg"));
#endif
}

Application::~Application() = default;

bool Application::initialize() {
    // Apply the theme before displaying any startup configuration error.
    QFile style(":/styles.qss");
    if (style.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(style.readAll()));
    else qWarning() << "Impossible de charger le style";
    SplashProcess splash;
    QString error;
    if (!AppSettings::initialize(&error)) {
        splash.finish();
        QMessageBox dialog(QMessageBox::Critical, tr("Configuration invalide"), tr("Configuration invalide"), QMessageBox::Ok);
        dialog.setTextFormat(Qt::PlainText);
        qsizetype first = 0;
        while (first < error.size() && error.at(first).isSpace()) ++first;
        error = error.mid(first);
        dialog.setInformativeText(error.isEmpty() ? tr("Impossible de charger les \u00e9tats.") : error);
        dialog.setTextInteractionFlags(Qt::TextSelectableByMouse);
        dialog.exec();
        return false;
    }
    // chargement du style et des traductions
    const auto inputPalette = setFocusPalette(palette(), QColor("#F75A1B"));
    setPalette(inputPalette, "QLineEdit");
    setPalette(inputPalette, "QDoubleSpinBox");
    if (translator_.load(":/qtbase_fr.qm")) installTranslator(&translator_);
    else qWarning() << "Impossible de charger les traductions Qt en francais";

    // Initialisation de la gestion série
    serial_ = std::make_unique<SerialController>();
    // Initialisation de l'interface graphique
    window_ = std::make_unique<MainWindow>(nullptr, serial_.get());
    window_->show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    splash.finish();
    return true;
}
