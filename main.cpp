#include "ui_eucaimwelcome.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QSslConfiguration>
#include <QSslCertificate>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/eucaim.png"));

    QDialog* dlg = new QDialog();
    Ui::WelcomeWindow* d = new Ui::WelcomeWindow;
    d->setupUi(dlg);
    QObject::connect(d->buttonBox, SIGNAL(accepted()), dlg, SLOT(accept()));

    MainWindow* mw = new MainWindow;
    QObject::connect(dlg, &QDialog::accepted, mw, [mw]() {
        mw->on_tokens(token_data{}, user_info{});
        mw->show();
        mw->raise();
    });
    dlg->show();
    return a.exec();
}
