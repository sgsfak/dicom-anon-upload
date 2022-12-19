#include "loginwindow.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/cardiocare-logo.ico"));
    LoginWindow* w = new LoginWindow;
    w->show();
    QObject::connect(w, &LoginWindow::tokens, w, [w](const token_data& tokens) {
        MainWindow* mw = new MainWindow;
        mw->on_tokens(tokens);
        mw->show();
        w->close();
        w->deleteLater();
    });
    return a.exec();
}
