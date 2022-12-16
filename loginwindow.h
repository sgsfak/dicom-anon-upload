#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>

#include "token_data.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginWindow; }
QT_END_NAMESPACE

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    LoginWindow(QWidget *parent = nullptr);
    virtual ~LoginWindow() override;

signals:
    void tokens(const token_data&);

private slots:
    void on_loginButton_clicked();

    void on_login_finished(class QNetworkReply*);

    void do_login(const QString& username, const QString& passwd);
    void displayError(int socketError, const QString &message);

    void on_passwordLineEdit_returnPressed();

    void on_usernameLineEdit_returnPressed();

private:
    Ui::LoginWindow *ui;
    QString host_;

    QString access_token_;
    QString refresh_token_;
};
#endif // LOGINWINDOW_H
