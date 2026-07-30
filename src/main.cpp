#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/filemanager.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
