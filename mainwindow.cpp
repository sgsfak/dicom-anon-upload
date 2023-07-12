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
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QPushButton>
#include <QProcess>

#include "worker.h"
#include "upload_worker.h"
#include "ui_progress_dialog.h"
#include "utils.h"

#define VERSION "0.8.0"

#define _STR(X) #X
#define STR(X) _STR(X)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    dlg_(nullptr)
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

    // Open the history db, where we store the uploads etc:
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "history_db");
    QString dbFile = qApp->applicationDirPath() + "/history.sqlite";
    db.setDatabaseName( dbFile );
    qDebug() << "Opening DB at" << dbFile;
    if (!db.open()) {
        QMessageBox::information(this, "Login", "Cannot open database at " + dbFile);
    }
    else {
        QSqlQuery q = db.exec(
                    "CREATE TABLE IF NOT EXISTS history("
                    " id INTEGER PRIMARY KEY,"
                    " patient_id TEXT NOT NULL,"
                    " timepoint_id TEXT NOT NULL,"
                    " timepoint TEXT NOT NULL,"
                    " anon_dir TEXT,"
                    " uploaded BOOLEAN DEFAULT false)");
        if (db.lastError().type() != QSqlError::NoError) {
            QMessageBox::information(this, "Error", "Cannot open history database at " + dbFile);
        }
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

namespace {
    qlonglong insert_history(const QString& patient_id, const QString& timepoint_id, const QString& timepoint, const QString& anon_dir)
    {
        QSqlQuery query{QSqlDatabase::database("history_db")};
        query.prepare("INSERT INTO history (patient_id, timepoint, anon_dir) "
                      "VALUES (:patient_id, :timepoint, :anon_dir)");
        query.bindValue(":patient_id", patient_id);
        query.bindValue(":timepoint_id", timepoint_id);
        query.bindValue(":timepoint", timepoint);
        query.bindValue(":anon_dir", anon_dir);
        query.exec();
        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << "SQL Error" << query.lastError();
            return 0;
        }

        auto history_id = query.lastInsertId().toLongLong();
        qDebug() << "inserted with " << history_id;
        return history_id;


    }
}
void MainWindow::anonymize(const QString &filePath, const QString& patId,
                           const QString& tmId, const QString& label)
{

    Worker* worker = new Worker(filePath, patId, tmId, label, this->tokens.access_token);

    this->upload_info.anon_folder = worker->temp_anon_folder();
    this->upload_info.patient_id = patId;
    this->upload_info.timepoint_id = tmId;

    QThread* workerThread = new QThread(this);
    worker->moveToThread(workerThread);



    QDialog* w = new QDialog(this);
    w->setWindowTitle("Progress");
    w->setModal(true);

    if (this->dlg_) delete this->dlg_;
    this->dlg_ = new Ui_Dialog();
    this->dlg_->setupUi(w);

    // I am using the following mapping from Roles to Actions:
    //   * Help -> open viewer for the user to inspect the Anonymized DICOM files
    //   * Apply -> upload anonymized files to server


    this->dlg_->buttonBox->button(QDialogButtonBox::Help)->setEnabled(false);
    this->dlg_->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);


    this->dlg_->buttonBox->button(QDialogButtonBox::Help)->setText("Inspect");
    this->dlg_->buttonBox->button(QDialogButtonBox::Apply)->setText("Upload");
    connect(this->dlg_->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button){
        QAbstractButton* inspectBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Help);
        QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);

       if (button == inspectBtn) {
           QProcess::startDetached(mdicom_path(), QStringList(this->upload_info.anon_folder));
       }
       else if (button == uploadBtn) {
           this->dlg_->progressBar->setVisible(true);
           this->upload();
       }
    });

    connect(workerThread, &QThread::started, worker, &Worker::anonymize);
    connect(worker, &Worker::error, worker, &Worker::deleteLater);

    connect(worker, &Worker::progress, this, [this](const QString& s) {
      this->dlg_->label->setText(s);
    });

    connect(worker, &Worker::error, this, [this] (const QString& error) {
        this->dlg_->label->setText("Error!! <br>" + error);
    });
    connect(worker, &Worker::finishedAnon, this, [this]() {
        this->dlg_->label->setText("<h2>Anonymization finished!</h2>"
                                   "You can view the anonymized files by pressing the Inspect button or proceed directly to Upload");
        this->dlg_->progressBar->setVisible(false);
        this->dlg_->progressBar->setRange(0, 1000);

        QAbstractButton* inspectBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Help);
        QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);

        inspectBtn->setEnabled(true);
        uploadBtn->setEnabled(true);
      });

    connect(w, &QDialog::finished, workerThread, &QThread::quit);
    connect(w, &QDialog::finished, &Worker::deleteLater);


    w->show();


    /*

    auto pd = new QProgressDialog(QObject::tr("Anonymizing .."), nullptr, 0, 1000, this);

    connect(workerThread, &QThread::started, worker, &Worker::anonymize);
    connect(worker, &Worker::progress, pd, &QProgressDialog::setLabelText);
    connect(worker, &Worker::uploadProgress1000, pd, &QProgressDialog::setValue);


//    connect(worker, &Worker::finishedAnon, worker, &Worker::upload);
    connect(worker, &Worker::finishedAnon, pd, &QProgressDialog::cancel);
    connect(worker, &Worker::finishedAnon, workerThread, &QThread::quit);

    connect(worker, &Worker::error, pd, &QProgressDialog::cancel);
    connect(worker, &Worker::error, worker, &Worker::deleteLater);

    connect(worker, &Worker::finished, workerThread, &QThread::quit);
    connect(worker, &Worker::finished, this->pd_, &QProgressDialog::cancel);

    // automatically delete thread and task object when work is done:
    connect(worker, &Worker::finished, worker, &Worker::deleteLater);

    connect(worker, &Worker::finished, this, [this, worker] (int n) {
            if (worker->success()) {
                ::insert_history(worker->patient_id(), worker->timepoint_id(), worker->timepoint(), worker->temp_anon_folder());
                QMessageBox::information(this, tr("Finished"),
                                         QString("Success: %1 DICOM images uploaded!").arg(n));
            }

        }
    );

    connect(worker, &Worker::error, this, [this] (const QString& error) {
        QMessageBox::warning(this, tr("Failed"), error);
        //pd->cancel();
    });
    */

    workerThread->start();
//    pd->exec();
//    int k = worker->
//    auto mesgBox = new QMessageBox("Anon")
//    QMessageBox::information(this, "Anonymization result", true? "OK" : "Error");
//    qDebug() << QThread::currentThread();
//    this->upload(anon_folder, patId, tmId);

}

void MainWindow::upload()
{
//    auto pd = new QProgressDialog(QObject::tr("Uploading .."), nullptr, 0, 1000, this);


    QDialog* w = qobject_cast<QDialog*>(this->dlg_->buttonBox->parent());

    QThread* workerThread = new QThread(this);
    UploadWorker* worker = new UploadWorker(this->tokens.access_token,
                                            this->upload_info.anon_folder,
                                            this->upload_info.patient_id,
                                            this->upload_info.timepoint_id);
    worker->moveToThread(workerThread);

//    worker->upload();

    connect(workerThread, &QThread::started, worker, &UploadWorker::upload);
    connect(worker, &UploadWorker::error, worker, &UploadWorker::deleteLater);


    connect(worker, &UploadWorker::error, this, [this] (const QString& error) {
        QMessageBox::warning(this, tr("Failed"), error);
        //pd->cancel();
    });

    connect(worker, &UploadWorker::uploadProgress1000, this, [this](int k) {
        this->dlg_->label->setText("<h2>Uploading...</h2>");
        this->dlg_->progressBar->setValue(k);
      });
    connect(worker, &UploadWorker::finished, this, [this]() {
        this->dlg_->progressBar->setVisible(false);
        this->dlg_->label->setText(QString("<h2>Upload finished!</h2>"
                                           "You can view the uploaded files "
                                           "<a href=\"https://dcm.cardiocare-project.eu/stone-webviewer/index.html?patient=%1\">here</a>.")
                                   .arg(this->upload_info.patient_id));
        QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);
        uploadBtn->setEnabled(false);
        this->dlg_->buttonBox->button(QDialogButtonBox::Cancel)->setText("OK");
      });

    connect(w, &QDialog::finished, workerThread, &QThread::quit);
    connect(w, &QDialog::finished, &Worker::deleteLater);

    /*

    connect(workerThread, &QThread::started, worker, &UploadWorker::upload);
//    connect(worker, &Worker::progress, this->pd_, &QProgressDialog::setLabelText);
    connect(worker, &UploadWorker::uploadProgress1000, pd, &QProgressDialog::setValue);

    connect(worker, &UploadWorker::finished, workerThread, &QThread::quit);
    connect(worker, &UploadWorker::finished, pd, &QProgressDialog::cancel);
    connect(worker, &UploadWorker::error, pd, &QProgressDialog::cancel);

    // automatically delete thread and task object when work is done:
    connect(worker, &UploadWorker::finished, worker, &Worker::deleteLater);
    connect(worker, &UploadWorker::error, worker, &Worker::deleteLater);

    connect(worker, &UploadWorker::finished, this, [this, worker] (int n) {
            if (worker->success()) {
//                ::insert_history(worker->patient_id(), worker->timepoint_id(), worker->timepoint(), worker->temp_anon_folder());
                QMessageBox::information(this, tr("Finished"),
                                         QString("Success: %1 DICOM images uploaded!").arg(n));
            }

        }
    );

    connect(worker, &UploadWorker::error, this, [this] (const QString& error) {
        QMessageBox::warning(this, tr("Failed"), error);
        //pd->cancel();
    });
    */

    workerThread->start();
//    pd->exec();
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

