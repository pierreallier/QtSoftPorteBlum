#pragma once
#include <QApplication>
#include <QTranslator>
#include <memory>
class MainWindow;
class SerialController;

class Application final : public QApplication {
    public:
        Application(int &argc, char **argv);
        ~Application() override;
        bool initialize();
    private:
        QTranslator translator_;
        std::unique_ptr<SerialController> serial_;
        std::unique_ptr<MainWindow> window_;
};
