#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qtbcrypt.h"
#include <QMessageBox>
#include <QDebug>
#include <QTcpSocket>
#include <QTextCodec>
#include <QTimer>
#include <QProgressDialog>
#include <QtSql>

#include "imph2mthread.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->centralwidget->setStyleSheet("background-color: white");
    ui->statusbar->setStyleSheet("background-color: white");


    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbFile = qApp->applicationDirPath()
            + QDir::separator()
            + "users.sqlite";
    db.setDatabaseName( dbFile );
    qDebug() << "Openning DB at" << dbFile;
    if (!db.open()) {
        QMessageBox::information(this, "Login", "Cannot open database at " + dbFile);
    }

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

#define Q_EXEC(q) \
    if (!q.exec()) \
      qDebug() << __FILE__ << ":" << __LINE__ << q.lastError().text() << q.lastQuery()

void MainWindow::do_login(const QString& username, const QString& passwd)
{
    QWidget* w = this;

    /*
    // Hash for password 'stelios'
    QString hash = "$2y$10$EvCPDFJaD8471NCmbR4S4O.QhBl30khbrPedZXk15skHxW7TaUYhO";
    QString hashedPassword = QtBCrypt::hashPassword(passwd, hash);
   */

    QSqlQuery q("SELECT hash FROM users WHERE username=?");
    q.addBindValue(username);
    Q_EXEC(q);
    bool success = false;
    if (q.next()) {
        QString hash = q.value("hash").toString();
        QString hashedPassword = QtBCrypt::hashPassword(passwd, hash);
        success = hash == hashedPassword;
    }

    if (success) {
        ui->usernameLineEdit->setText("");
        ui->passwordLineEdit->setText("");
    }
    QMessageBox::information(w, "Login", success ? "Login successfull!" : "Wrong username or password");


}

void MainWindow::on_loginButton_clicked()
{
    auto username = ui->usernameLineEdit->text();
    auto passwd = ui->passwordLineEdit->text();

    do_login(username, passwd);
}

void MainWindow::on_panaceaButton_clicked()
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

void MainWindow::on_passwordLineEdit_returnPressed()
{
    this->on_loginButton_clicked();
}


void MainWindow::on_usernameLineEdit_returnPressed()
{
    this->on_loginButton_clicked();
}

