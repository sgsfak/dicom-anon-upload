#include "loginwindow.h"
#include "ui_loginwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QProgressDialog>
#include <QtSql>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>


LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LoginWindow)
    , host_("https://cardiocare.ics.forth.gr/cardiocare/oauth2")
{
    ui->setupUi(this);
    QFont font = ui->loginLabel->font();
    font.setPointSize(22);
    ui->loginLabel->setFont(font);
//    ui->centralwidget->setStyleSheet("background-color: white");
//    ui->statusbar->setStyleSheet("background-color: white");


    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbFile = qApp->applicationDirPath() + "/config.sqlite";
    db.setDatabaseName( dbFile );
    qDebug() << "Opening DB at" << dbFile;
    if (!QFileInfo::exists(dbFile) || !db.open()) {
        QMessageBox::information(this, "Login", "Cannot open database at " + dbFile);
    }

}

LoginWindow::~LoginWindow()
{
    delete ui;
}


#define Q_EXEC(q) \
    if (!q.exec()) \
      qDebug() << __FILE__ << ":" << __LINE__ << q.lastError().text() << q.lastQuery()


void LoginWindow::do_login(const QString& username, const QString& passwd)
{
    QWidget* w = this;


    QSqlQuery q;
    q.prepare("SELECT client_id, client_secret FROM oauth_config limit 1");
    Q_EXEC(q);
    if (q.next()) {
        QString client_id = q.value(0).toString();
        QString client_secret = q.value(1).toString();
        QNetworkRequest request;
        request.setUrl(QUrl(this->host_ + "/token/"));
        qDebug() << "LOGIN: sending to" << this->host_;
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

        QNetworkAccessManager *qnam = new QNetworkAccessManager(this);

        this->progress_ = new QProgressDialog("Signing in...", "Abort", 0, 0, this);
        this->progress_->setValue(0);
        this->progress_->setMinimumDuration(1000);

        connect(qnam, SIGNAL(finished(QNetworkReply*)),this, SLOT(on_login_finished(QNetworkReply*)));
        // connect(qnam, &QNetworkAccessManager::finished, this, &LoginWindow::on_login_finished);


        qnam->post(request,
                  QString("grant_type=password&username=%1&password=%2&client_id=%3&client_secret=%4").arg(username, passwd, client_id, client_secret).toUtf8());

        return;
    }

    QMessageBox::warning(w, "Initialization", "Could not load config!");

}

void LoginWindow::on_loginButton_clicked()
{
    auto username = ui->usernameLineEdit->text();
    auto passwd = ui->passwordLineEdit->text();

    do_login(username, passwd);
}

void LoginWindow::on_login_finished(QNetworkReply* reply)
{

    this->progress_->cancel();

    // At the end of that slot, we won't need it anymore
    reply->deleteLater();

    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status_code.isValid()) {
        QMessageBox::critical(this, "Login", QString("Error: %1").arg(reply->errorString()));
        return;
    }
    if (status_code.toInt() / 100 != 2) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray();
        QMessageBox::critical(this, "Authentication Failure",
                              QString("Login failed, error: %1\n\nPlease check your username and password.").arg(reason));
        return;
    }

    QByteArray result = reply->readAll();
    QJsonObject json = QJsonDocument::fromJson(result).object();
    token_data tokens;
    tokens.access_token = json.value("access_token").toString();
    tokens.refresh_token = json.value("refresh_token").toString();
    tokens.expires_in = json.value("expires_in").toInt();
    this->tokens_ = tokens;
    qDebug() << "Got tokens" << tokens.access_token;
    this->get_userinfo();
    /*
    emit this->tokens(tokens);

    // qDebug().noquote() << "Got tokens:" << this->access_token_ << this->refresh_token_;

    this->hide();
    */

}


void LoginWindow::get_userinfo()
{
    QNetworkRequest request;
    request.setUrl(QUrl(this->host_ + "/userinfo/"));
    QString authHeader = QString("Bearer %1").arg(this->tokens_.access_token);
    request.setRawHeader("Authorization", authHeader.toUtf8());

    QNetworkAccessManager *qnam = new QNetworkAccessManager(this);

    this->progress_ = new QProgressDialog("Getting user info...", "Abort", 0, 0, this);
    this->progress_ ->setValue(0);
    this->progress_ ->setMinimumDuration(1000);

    connect(qnam, SIGNAL(finished(QNetworkReply*)),this, SLOT(on_userinfo_finished(QNetworkReply*)));

    qnam->get(request);
}


void LoginWindow::on_userinfo_finished(QNetworkReply* reply)
{

    this->progress_->cancel();

    // At the end of that slot, we won't need it anymore
    reply->deleteLater();

    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status_code.isValid()) {
        QMessageBox::critical(this, "Get User Info", QString("Error: %1").arg(reply->errorString()));
        return;
    }
    if (status_code.toInt() / 100 != 2) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray();

        QMessageBox::critical(this, "Get User Info Failure",
                              QString("Getting user information failed, error: %1").arg(reason));
        return;
    }

    QByteArray result = reply->readAll();
    qDebug().noquote() << "UserInfo:" << QString(result);
    QJsonObject json = QJsonDocument::fromJson(result).object();
    this->user_.user_id = json.value("sub").toString();
    this->user_.name = json.value("name").toString();

    QJsonArray json_array = json.value("groups").toArray();
    for(const QJsonValue& v: std::as_const(json_array)) {
        this->user_.groups.push_back(v.toString());
    }

    emit this->tokens(this->tokens_, this->user_);

    this->hide();
}

void LoginWindow::displayError(int socketError, const QString &message)
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

void LoginWindow::on_passwordLineEdit_returnPressed()
{
    this->on_loginButton_clicked();
}


void LoginWindow::on_usernameLineEdit_returnPressed()
{
    this->on_loginButton_clicked();
}

