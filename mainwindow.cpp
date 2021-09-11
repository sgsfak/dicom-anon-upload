#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qtbcrypt.h"
#include <QMessageBox>
#include <QDebug>
#include <QTcpSocket>
#include <QTextCodec>
#include <QTimer>
#include <QProgressDialog>

#include "imph2mthread.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->centralwidget->setStyleSheet("background-color: white");
    ui->statusbar->setStyleSheet("background-color: white");
}

MainWindow::~MainWindow()
{
    delete ui;
}

namespace {
void hash_password(QWidget* w, const QString& username, const QString& passwd)

{
    QString salt = QtBCrypt::generateSalt();
    QString hashedPassword = QtBCrypt::hashPassword(passwd, salt);

    qDebug() <<"Username" << username << "Hash" << hashedPassword;
    QMessageBox::information(w, "bcrypt Hash", "Username:" + username + "\nHash:" + hashedPassword);
}

}

void MainWindow::do_login(const QString& username, const QString& passwd)
{
    QWidget* w = this;
    // Hash for password 'stelios'
    QString hash = "$2y$10$EvCPDFJaD8471NCmbR4S4O.QhBl30khbrPedZXk15skHxW7TaUYhO";
    QString hashedPassword = QtBCrypt::hashPassword(passwd, hash);

    qDebug() << "Hash" << hashedPassword;
    QMessageBox::information(w, "Login", hash == hashedPassword ? "Login successfull!" : "Wrong username or password");
}

void MainWindow::on_pushButton_clicked()
{
    auto username = ui->usernameLineEdit->text();
    auto passwd = ui->passwordLineEdit->text();

    do_login(username, passwd);
}


namespace {
inline void delay(int millisecondsWait)
{
    QEventLoop loop;
    QTimer t;
    t.connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(millisecondsWait);
    loop.exec();
}

QString send_command(QWidget* w, const char* command)
{

    QTcpSocket sock;
    sock.connectToHost("127.0.0.1", 27016);
    if (!sock.waitForConnected()) {
        QMessageBox::critical(w, "Error", "Cannot connect to IMP H2M services...");
        return "";
    }

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");

    sock.write(command);
    QStringList sl;
    forever {
        sock.waitForReadyRead();
        QByteArray ba = sock.readAll();
        if (ba.isEmpty())
            break;
        QString r = codec->toUnicode(ba).simplified();
        qDebug() << "SOCK ->" << r;
        sl.append(r);

    }
    return sl.join("\n");
}
}
void MainWindow::on_pushButton_2_clicked()
{
    QProgressDialog* progress = new QProgressDialog("Camera capture", "Abort capture", 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setValue(0);

    ImpH2MThread* thread = new ImpH2MThread(this);
    connect(thread, &ImpH2MThread::error, this, &MainWindow::displayError);
    connect(thread, &ImpH2MThread::credentials, this, &MainWindow::do_login);
    connect(thread, &ImpH2MThread::error, progress, &QProgressDialog::cancel);
    connect(thread, &ImpH2MThread::credentials, progress, &QProgressDialog::cancel);
    connect(progress, &QProgressDialog::canceled, thread, &ImpH2MThread::stop);
    thread->requestNewCapture();
}

void MainWindow::displayError(int socketError, const QString &message)
{
    switch (socketError) {
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, tr("ICS Login"),
                                 tr("The host was not found. Please check the "
                                    "host and port settings."));
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, tr("ICS Login"),
                                 tr("The connection was refused by the peer. "
                                    "Make sure the IMP H2M service is running, "
                                    "and check that the host name and port "
                                    "settings are correct."));
        break;
    default:
        QMessageBox::information(this, tr("ICS Login"),
                                 tr("The following error occurred: %1.")
                                 .arg(message));
    }
}
