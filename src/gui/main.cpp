#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>

#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("MineSleuth");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("MineSleuth");

    // Load bundled QSS Stylesheet
    QFile styleFile(":/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    // Set default geometric Futura typography
    QFont font("Futura", 10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    minesleuth::gui::MainWindow window;
    window.show();

    // If a file argument was passed, open it immediately
    if (argc > 1) {
        window.openFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
