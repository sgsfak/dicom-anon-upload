#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPair>
#include <vector>

#include "token_data.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;


protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void anonymize(const QString& folder, const QString& patId, const QString& tmId, const QString& label);

public slots:
    void on_tokens(const token_data& t, const user_info& u);

private slots:
    void on_action_About_triggered();

private:
    Ui::MainWindow *ui;
    token_data tokens;
    QString style;

    std::vector<QPair<QString, QString>> timepoints_;
};

#endif // MAINWINDOW_H
