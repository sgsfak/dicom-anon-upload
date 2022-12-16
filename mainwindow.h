#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void anonymize(const QString& folder);

public slots:
    void on_tokens(const token_data& t);

private:
    Ui::MainWindow *ui;
    token_data tokens;
};

#endif // MAINWINDOW_H
