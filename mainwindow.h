#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_loginButton_clicked();

    void on_panaceaButton_clicked();

    void do_login(const QString& username, const QString& passwd);
    void displayError(int socketError, const QString &message);

    void on_passwordLineEdit_returnPressed();

    void on_usernameLineEdit_returnPressed();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
