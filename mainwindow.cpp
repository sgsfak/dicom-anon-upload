#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qmessagebox.h>
#include <qdebug.h>
#include "qtbcrypt.h"

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
    void do_login(QWidget* w, const QString& username, const QString& passwd)
{
    // Hash for password 'stelios'
    QString hash = "$2y$10$EvCPDFJaD8471NCmbR4S4O.QhBl30khbrPedZXk15skHxW7TaUYhO";
    QString hashedPassword = QtBCrypt::hashPassword(passwd, hash);

    qDebug() << "Hash" << hashedPassword;
    QMessageBox::information(w, "Login", hash == hashedPassword ? "Login successfull!" : "Wrong username or password");
}
}

void MainWindow::on_pushButton_clicked()
{
    auto username = ui->usernameLineEdit->text();
    auto passwd = ui->passwordLineEdit->text();

    do_login(this, username, passwd);
}
