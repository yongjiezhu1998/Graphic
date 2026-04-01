#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DspTunerHost"));
    app.setOrganizationName(QStringLiteral("DspTunerHost"));
    app.setApplicationDisplayName(QStringLiteral("DSP调音上位机"));

    MainWindow window;
    window.show();

    return app.exec();
}
