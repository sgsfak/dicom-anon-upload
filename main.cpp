#include "loginwindow.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/cardiocare-logo.ico"));
    LoginWindow* w = new LoginWindow;
    MainWindow* mw = new MainWindow;
//    mw->hide();
    mw->show();
    w->show();
    QObject::connect(w, &LoginWindow::tokens, mw, &MainWindow::on_tokens);
    return a.exec();
}
