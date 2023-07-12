#ifndef HISTORYFORM_H
#define HISTORYFORM_H

#include <QWidget>

namespace Ui {
class HistoryForm;
}

class HistoryForm : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryForm(QWidget *parent = nullptr);
    ~HistoryForm();

private slots:
    void on_historyTable_itemDoubleClicked(class QTableWidgetItem *item);

private:
    Ui::HistoryForm *ui;
};

#endif // HISTORYFORM_H
