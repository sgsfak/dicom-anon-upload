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

private:

    void do_login(const QString& username, const QString& passwd);
    void get_userinfo();

signals:
    void tokens(const token_data&, const user_info& user);

private slots:
    void on_loginButton_clicked();

    void on_login_finished(class QNetworkReply*);
    void on_userinfo_finished(class QNetworkReply*);

    void displayError(int socketError, const QString &message);

    void on_passwordLineEdit_returnPressed();

    void on_usernameLineEdit_returnPressed();

private:
    Ui::LoginWindow *ui;
    QString host_;

    class QProgressDialog* progress_;

    token_data tokens_;
    user_info user_;
};
#endif // LOGINWINDOW_H
