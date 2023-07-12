#include "loginwindow.h"
#include "mainwindow.h"
#include "utils.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>

int main(int argc, char *argv[])
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/cardiocare-logo.ico"));

    QFileInfo mdicom_exe {mdicom_path()};
    if (!mdicom_exe.exists() || !mdicom_exe.isFile() || !mdicom_exe.isExecutable()) {
        QMessageBox::information(nullptr, "mDicom.exe missing",
                                 "Installation of MicroDicom cannot be found"
                                 " so viewing of DICOM files is not supported. <br>If you want it, "
                                 " you can download its installer from <a href='https://www.microdicom.com/downloads.html'>here</>.");
    }

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
    return a.exec();
}
