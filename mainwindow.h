#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPair>
#include <vector>

#include "token_data.h"

namespace Ui {
class MainWindow;
}

struct current_upload_info {
    QString anon_folder;
    QString patient_id;
    QString timepoint_id;
};

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

public slots:
    void on_tokens(const token_data& t, const user_info& u);

private slots:
    void on_action_About_triggered();
    void anonymize(const QString& folder, const QString& patId, const QString& tmId, const QString& label);
    void upload();

private:
    Ui::MainWindow *ui;
//    class QProgressDialog* pd_;
    class Ui_Dialog* dlg_;
    current_upload_info upload_info;

    token_data tokens;
    QString style;

    std::vector<QPair<QString, QString>> timepoints_;
};

#endif // MAINWINDOW_H
