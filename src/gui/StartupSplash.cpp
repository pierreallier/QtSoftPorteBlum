#include "gui/StartupSplash.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QSplashScreen>
#include <QTimer>
#include <QUuid>
#include <QDebug>
#include <cstdio>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class StartupSplash final : public QSplashScreen {
public:
    explicit StartupSplash(const QPixmap &image) : QSplashScreen(image) {
        elapsed_.start();
        timer_.setInterval(40);
        connect(&timer_, &QTimer::timeout, this, [this] { update(); });
        timer_.start();
    }
protected:
    void drawContents(QPainter *painter) override {
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF spinner(22, height() - 37, 16, 16);
        painter->setPen(QPen(QColor("#E5E7EB"), 2));
        painter->drawEllipse(spinner);
        painter->setPen(QPen(QColor("#D97706"), 2, Qt::SolidLine, Qt::RoundCap));
        const int angle = static_cast<int>((elapsed_.elapsed() % 1000) * 360 / 1000);
        painter->drawArc(spinner, -angle * 16, 100 * 16);
        painter->setPen(QColor("#333333"));
        painter->drawText(QRect(46, height() - 42, width() - 68, 26),
                          Qt::AlignLeft | Qt::AlignVCenter, tr("Chargement"));
    }
    void mousePressEvent(QMouseEvent *) override {}
private:
    QTimer timer_;
    QElapsedTimer elapsed_;
};
}

SplashProcess::SplashProcess()
    : state_(QStringLiteral("SoftPorteBlum-splash-") + QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    if (!state_.create(1)) {
        qWarning() << "Impossible de préparer le splashscreen:" << state_.errorString();
        return;
    }
    if (!state_.lock()) return;
    *static_cast<char *>(state_.data()) = 0;
    state_.unlock();
#ifdef Q_OS_WIN
    // Keep the helper's console hidden; only its splash window is shown.
    process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process_.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QObject::connect(&process_, &QProcess::errorOccurred, &process_, [this](QProcess::ProcessError error) {
        if (!finishing_ && (error == QProcess::FailedToStart || error == QProcess::Crashed))
            qWarning() << "Splashscreen indisponible:" << process_.errorString();
    });
    QObject::connect(&process_, &QProcess::readyReadStandardOutput, &process_, [this] {
        process_.readAllStandardOutput();
    });
    // The helper has its own event loop: do not block application startup waiting
    // for its window. If startup finishes first, the shared flag prevents a late splash.
    process_.start(QCoreApplication::applicationFilePath(), {"--startup-splash", state_.key()});
}

SplashProcess::~SplashProcess() {
    finish();
}

void SplashProcess::finish() {
    finishing_ = true;
    if (state_.isAttached() && state_.lock()) {
        *static_cast<char *>(state_.data()) = 1;
        state_.unlock();
    }
    if (process_.state() != QProcess::NotRunning && !process_.waitForFinished(1500)) {
        process_.kill();
        process_.waitForFinished(1000);
    }
}

int runStartupSplash(const QString &sharedMemoryKey) {
    QSharedMemory state(sharedMemoryKey);
    if (!state.attach(QSharedMemory::ReadOnly)) return 2;
    const auto finished = [&state] {
        if (!state.lock()) return true;
        const bool done = *static_cast<const char *>(state.constData()) != 0;
        state.unlock();
        return done;
    };
    if (finished()) return 0;

    QPixmap image(480, 260);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        QIcon(":/icons/splashscreen.svg").paint(&painter, QRect(60, 35, 360, 150));
    }
    StartupSplash splash(image);
    splash.show();
    QFile output;
    if (output.open(stdout, QIODevice::WriteOnly)) {
        output.write("ready\n");
        output.flush();
    }
    QTimer completion;
    completion.setInterval(50);
    QObject::connect(&completion, &QTimer::timeout, &splash, [&] {
        if (finished()) QApplication::quit();
    });
    completion.start();
    return QApplication::exec();
}
