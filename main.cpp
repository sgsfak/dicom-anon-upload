#include "ui_eucaimwelcome.h"
#include "mainwindow.h"
#include "utils.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <iostream>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/eucaim.png"));
#if 0
    qDebug() << "SSL Build Version:" << QSslSocket::sslLibraryBuildVersionString();
    QString certsPath = a.applicationDirPath() + "/certs/";
    std::cout << "looking at " << certsPath.toStdString() << " for any PEM certificate" << std::endl;
    QDir certsDir (certsPath);
    QStringList certList = certsDir.entryList(QStringList() << "*.pem",QDir::Files);
    foreach (const QString &s, certList){

        std::cout << "loading " << (certsPath+s).toStdString() << std::endl;
        const auto certs = QSslCertificate::fromPath((certsPath+s));
        if (certs.size() < 1) {
            std::cout << "can't add  " << (certsPath+s).toStdString() << std::endl;
        }
        else {
            for (const QSslCertificate & cert: certs) {
                qDebug() << "Adding cert" << cert.issuerInfo((QSslCertificate::Organization));
                QSslConfiguration::defaultConfiguration().addCaCertificate(cert);
            }
        }
    }
#endif

    QFileInfo mdicom_exe {mdicom_path()};
    if (!mdicom_exe.exists() || !mdicom_exe.isFile() || !mdicom_exe.isExecutable()) {
        QMessageBox::information(nullptr, "mDicom.exe missing",
                                 "Installation of MicroDicom cannot be found"
                                 " so viewing of DICOM files is not supported. <br>If you want it, "
                                 " you can download its installer from <a href='https://www.microdicom.com/downloads.html'>here</>.",
                                 QMessageBox::Ok);
    }

#if 0
    LoginWindow* w = new LoginWindow;
    w->show();
    QObject::connect(w, &LoginWindow::tokens, w, [w](const token_data& tokens, const user_info& user) {
        MainWindow* mw = new MainWindow;
        mw->on_tokens(tokens, user);
        mw->show();
        mw->raise();
        w->close();
        w->deleteLater();
    });
#else


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
    // // if (dlg->exec() == QDialog::Accepted) {
    // //     MainWindow* mw = new MainWindow;
    // //     // mw->on_tokens(tokens, user);
    // //     mw->show();
    // //     mw->raise();
    // // }
    dlg->show();
    return a.exec();
#endif
}
