#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ui_patientinfo.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDir>
#include <QThread>
#include <QTimer>
#include <QLabel>
#include <QSqlQuery>

#include "worker.h"
#define VERSION "0.7.0"

#define _STR(X) #X
#define STR(X) _STR(X)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    this->setUnifiedTitleAndToolBarOnMac(true);
    ui->setupUi(this);
    this->setAcceptDrops(true);
    this->style = this->styleSheet();

    QSqlQuery q;
    q.prepare("SELECT id, descr FROM timepoints");
    q.exec();
    while (q.next()) {
        this->timepoints_.emplace_back(q.value(0).toString(), q.value(1).toString());
    }
}


void MainWindow::on_tokens(const token_data& tokens, const user_info& user) {
    this->tokens = tokens;
    QLabel *label = new QLabel(this);
//    label->setText("Git rev:" STR(APP_REVISION));
    label->setText(QString("User: %1").arg(user.name));
    this->statusBar()->addWidget(label);
    this->show();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    qDebug() << "dropEvent" << event->mimeData()->urls();
    setStyleSheet (this->style);
    this->statusBar()->clearMessage();

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
       return;

    QString fileName = urls.first().toLocalFile();
    if (fileName.isEmpty())
       return;

    QFileInfo qf(fileName);
    if (!qf.isDir()) {
        return;
    }


    event->accept();

    QDialog dlg(this);
    Ui::patientInfo d;
    d.setupUi(&dlg);
    for(const auto& s: this->timepoints_) {
       d.timepointComboBox->addItem(s.second, s.first);
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString patId = d.patientIDLineEdit->text();
        QString timePointId = d.timepointComboBox->currentData(Qt::UserRole).toString();
        QString timePointAnnotation = d.timepointComboBox->currentText();
        QTimer::singleShot(0, this, [this, fileName, patId, timePointId, timePointAnnotation]() {
            this->anonymize(fileName, patId, timePointId, timePointAnnotation);
        });
    }

}
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    qDebug() << mimeData->urls();
    if (mimeData->hasUrls() && mimeData->urls().constFirst().isLocalFile()) {
        this->statusBar()->showMessage(tr("Accepting.."));
        setStyleSheet ("background-color: rgba(173, 173, 173, 0.7);");
        event->acceptProposedAction();
    }

}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
   event->accept ();

   setStyleSheet (this->style);
}

void MainWindow::anonymize(const QString &filePath, const QString& patId,
                           const QString& tmId, const QString& label)
{
    QProgressDialog* pd = new QProgressDialog(QObject::tr("Anonymizing .."), nullptr, 0, 1000, this);

    QThread* workerThread = new QThread(this);
    Worker* worker = new Worker(filePath, patId, tmId, label, this->tokens.access_token);
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &Worker::anonymizeAndUpload);
    connect(worker, &Worker::progress, pd, &QProgressDialog::setLabelText);
    connect(worker, &Worker::uploadProgress1000, pd, &QProgressDialog::setValue);

    connect(worker, &Worker::finished, workerThread, &QThread::quit);
    connect(worker, &Worker::finished, pd, &QProgressDialog::cancel);
    connect(worker, &Worker::error, pd, &QProgressDialog::cancel);

    // automatically delete thread and task object when work is done:
    connect(worker, &Worker::finished, worker, &Worker::deleteLater);
    connect(worker, &Worker::error, worker, &Worker::deleteLater);
//    connect(workerThread, &QThread::started, workerThread, &QThread::deleteLater);

    connect(worker, &Worker::finished, this, [this, worker] (int n) {
            if (worker->success()) {
                QMessageBox::information(this, tr("Finished"),
                                         QString("Success: %1 DICOM images uploaded!").arg(n));
            }
        }
    );

    connect(worker, &Worker::error, this, [this] (const QString& error) {
        QMessageBox::warning(this, tr("Failed"), error);
        //pd->cancel();
    });

    workerThread->start();
    pd->exec();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_action_About_triggered()
{
    QMessageBox::information(this, "About DICOM Upload Tool",
                             "<h1>Cardiocare DICOM Upload tool</h1>"
                             "Version: " VERSION "<br>"
                             "&copy; FORTH-ICS, 2023");
}

