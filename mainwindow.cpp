#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ui_patientinfo.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDropEvent>
#include <QFile>
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
#include <QFileDialog>
#include <QUuid>

#include "worker.h"
#include "upload_worker.h"
#include "ui_progress_dialog.h"
#include "utils.h"

#define VERSION QT_STRINGIFY(APP_VERSION)
#define GIT_REV QT_STRINGIFY(APP_REVISION)
#define CTP_URL "https://mircwiki.rsna.org/index.php?title=The_CTP_DICOM_Anonymizer"

static int qfile_create_if_needed(const QString& filename)
{

    QFile file(filename);
    if (file.exists())
        return 0;
    file.open(QIODevice::WriteOnly);
    file.close();
    return 1;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    dlg_(nullptr)
{
    this->setUnifiedTitleAndToolBarOnMac(true);
    ui->setupUi(this);
    this->setAcceptDrops(true);
    this->style = this->styleSheet();

    this->setWindowTitle("EUCAIM DICOM Anonymizer");

    QSqlQuery q;
    q.prepare("SELECT id, descr FROM timepoints");
    q.exec();
    while (q.next()) {
        this->timepoints_.emplace_back(q.value(0).toString(), q.value(1).toString());
    }

    // Open the history db, where we store the uploads etc:
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "history_db");
    QString dbFile = qApp->applicationDirPath() + "/history.sqlite";
    ::qfile_create_if_needed(dbFile);
    db.setDatabaseName( dbFile );
    qDebug() << "Opening DB at" << dbFile;
    if (!db.open()) {
        QMessageBox::information(this, "Login", "Cannot open database at " + dbFile);
    }
    else {
        QSqlQuery q{db};
        q.exec("CREATE TABLE IF NOT EXISTS history("
               " id INTEGER PRIMARY KEY,"
               " patient_id TEXT NOT NULL,"
               " timepoint_id TEXT NOT NULL,"
               " timepoint TEXT NOT NULL,"
               " anon_dir TEXT NOT NULL,"
               " created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
               " error_msg TEXT DEFAULT NULL,"
               " upload_id TEXT DEFAULT NULL,"
               " upload_started_at DATETIME DEFAULT NULL,"
               " upload_finished_at DATETIME DEFAULT NULL)");
        if (db.lastError().type() != QSqlError::NoError) {
            QMessageBox::information(this, "Error", "Cannot open history database at " + dbFile);
        }
    }
}


void MainWindow::on_tokens(const token_data& tokens, const user_info& user) {
    this->tokens = tokens;
    this->user = user;
    QLabel *label = new QLabel(this);
    // label->setText("Git rev:" QT_STRINGIFY(APP_REVISION));
    label->setText(QString("Version %1 - User: %2").arg(VERSION, user.name));
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
    this->start_anonymize(fileName);

}

bool MainWindow::patientid_valid(const QString & patient_id) const
{
#if 0
    // Ids should conform to the following syntax:
    // <digit> <digit> "-" <digit> <digit>*

    static QRegularExpression re{"\\d\\d-\\d+"};
    if (!re.match(patientId).hasMatch()) {
        return false;
    }

    // we check the groups the user belongs to, and see whether
    // the given patient id is compatible with the corresponding
    // prefix:
    QString prefix = patientId.left(2);
    for(const QString& cc: this->user.groups) {
        if (!cc.startsWith("CC")) continue;
        else if (cc == "CC_TEST" && prefix == "00") return true;
        else if (cc == "CC_BOCOC" && prefix == "01") return true;
        else if (cc == "CC_IEO" && prefix == "02") return true;
        else if (cc == "CC_IOL" && prefix == "03") return true;
        else if (cc == "CC_KSBC" && prefix == "04") return true;
        else if (cc == "CC_NKUA" && prefix == "05") return true;
        else if (cc == "CC_UOI" && prefix == "06") return true;
    }
    return false;
#else
    Q_UNUSED(patient_id)
    return true;
#endif
}
void MainWindow::start_anonymize(const QString& dirName)
{

#if 0
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

        // Check the given patient id:
        if (!this->patientid_valid(patId)) {
            QMessageBox::critical(this,
                                  "Error",
                                  QString("The given patient id: '%1' does not appear to"
                                          " be valid for your clinical center!").arg(patId));
        }
        else
            QTimer::singleShot(0, this, [this, dirName, patId, timePointId, timePointAnnotation]() {
                this->anonymize(dirName, patId, timePointId, timePointAnnotation);
            });
    }
#else
    QString patId = QString("00-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString timePointId = "";
    QString timePointAnnotation = "";
    QTimer::singleShot(0, this, [this, dirName, patId, timePointId, timePointAnnotation]() {
        this->anonymize(dirName, patId, timePointId, timePointAnnotation);
    });
#endif
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
    qlonglong history_insert(const QString& patient_id, const QString& timepoint_id, const QString& timepoint, const QString& anon_dir)
    {
        QSqlQuery query{QSqlDatabase::database("history_db")};
        query.prepare("INSERT INTO history (patient_id, timepoint_id, timepoint, anon_dir) "
                      "VALUES (:patient_id, :timepoint_id, :timepoint, :anon_dir)");
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
    void history_set_upload_start(qlonglong history_id, const QString& upload_id)
    {
        QSqlQuery query{QSqlDatabase::database("history_db")};
        query.prepare("UPDATE history SET upload_id=:upload_id, upload_started_at=CURRENT_TIMESTAMP WHERE id=:id");
        query.bindValue(":upload_id", upload_id);
        query.bindValue(":id", history_id);
        query.exec();
        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << "SQL Error" << query.lastError();
        }
    }
    void history_set_upload_end(qlonglong history_id)
    {
        QSqlQuery query{QSqlDatabase::database("history_db")};
        query.prepare("UPDATE history SET upload_finished_at=CURRENT_TIMESTAMP WHERE id=:id");
        query.bindValue(":id", history_id);
        query.exec();
        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << "SQL Error" << query.lastError();
        }
    }
    void history_set_upload_error(qlonglong history_id, const QString& error)
    {
        QSqlQuery query{QSqlDatabase::database("history_db")};
        query.prepare("UPDATE history SET error_msg=:err WHERE id=:id");
        query.bindValue(":err", error);
        query.bindValue(":id", history_id);
        query.exec();
        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << "SQL Error" << query.lastError();
        }
    }
}
void MainWindow::anonymize(const QString &filePath, const QString& patId,
                           const QString& tmId, const QString& label)
{

    Worker* worker = new Worker(filePath, patId, tmId, label, this->tokens.access_token);

    this->upload_info.anon_folder = worker->temp_anon_folder();
    this->upload_info.patient_id = patId;
    this->upload_info.timepoint_id = tmId;
    this->upload_info.timepoint = label;


    this->upload_info.history_id = ::history_insert(this->upload_info.patient_id, this->upload_info.timepoint_id,
                                                    this->upload_info.timepoint, this->upload_info.anon_folder);

    QThread* workerThread = new QThread(this);
    worker->moveToThread(workerThread);


    QDialog* w = new QDialog(this);
    w->setWindowTitle("Progress");
    w->setModal(true);

    if (this->dlg_) delete this->dlg_;
    this->dlg_ = new Ui_Dialog();
    this->dlg_->setupUi(w);
    this->dlg_->label->setText("<h2>Anonymizing..</h2>");

    // I am using the following mapping from Roles to Actions:
    //   * Help -> open viewer for the user to inspect the Anonymized DICOM files
    //   * Apply -> upload anonymized files to server


    this->dlg_->buttonBox->button(QDialogButtonBox::Help)->setEnabled(false);
    this->dlg_->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);


    this->dlg_->buttonBox->button(QDialogButtonBox::Help)->setText("Inspect");
    this->dlg_->buttonBox->button(QDialogButtonBox::Apply)->setText("Open output folder");
    connect(this->dlg_->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button){
        QAbstractButton* inspectBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Help);
        QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);

       if (button == inspectBtn) {
           QProcess::startDetached(mdicom_path(), QStringList(this->upload_info.anon_folder));
       }
       else if (button == uploadBtn) {
#if 0
           this->upload();
#else
           QDesktopServices::openUrl(QString("file:%1").arg(this->upload_info.anon_folder));
#endif
       }
    });

    connect(workerThread, &QThread::started, worker, &Worker::anonymize);
    connect(worker, &Worker::error, worker, &Worker::deleteLater);

    connect(worker, &Worker::error, this, [this] (const QString& error) {
        this->dlg_->label->setText("<h2>Error!!</h2>" + error);
        this->dlg_->progressBar->setVisible(false);
    });
    connect(worker, &Worker::finishedAnon, this, [this]() {
        this->dlg_->label->setText("<h2>Anonymization finished!</h2>"
                                   "You can view the anonymized files by pressing the Inspect button or proceed directly to Upload");
        this->dlg_->progressBar->setVisible(false);

        QAbstractButton* inspectBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Help);
        QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);

        inspectBtn->setEnabled(true);
        uploadBtn->setEnabled(true);
      });

    connect(w, &QDialog::finished, workerThread, &QThread::quit);
    connect(w, &QDialog::finished, &Worker::deleteLater);


    w->show();


    workerThread->start();

}

void MainWindow::upload()
{


    QDialog* w = qobject_cast<QDialog*>(this->dlg_->buttonBox->parent());

    this->dlg_->label->setText("<h2>Preparing upload..</h2>");

    QThread* workerThread = new QThread(this);
    UploadWorker* worker = new UploadWorker(this->tokens.access_token,
                                            this->upload_info.anon_folder,
                                            this->upload_info.patient_id,
                                            this->upload_info.timepoint_id);
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &UploadWorker::upload);
    connect(worker, &UploadWorker::error, worker, &UploadWorker::deleteLater);


    connect(worker, &UploadWorker::error, this, [this] (const QString& upload_id, const QString& error) {
        ::history_set_upload_error(this->upload_info.history_id, error);
        this->dlg_->label->setText(QString("<h2>Error (upload_id: %1)</h2>%2").arg(upload_id, error));
        this->dlg_->progressBar->setVisible(false);
    });

    connect(worker, &UploadWorker::started, this, [this](const QString& upload_id) {
        ::history_set_upload_start(this->upload_info.history_id, upload_id);
        this->dlg_->label->setText("<h2>Uploading...</h2>");
        this->dlg_->progressBar->setVisible(true);
        this->dlg_->progressBar->setRange(0, 1000);
        this->dlg_->progressBar->setValue(0);
      });

    connect(worker, SIGNAL(uploadProgress1000(int)), this->dlg_->progressBar, SLOT(setValue(int)));

    connect(worker, &UploadWorker::finished, this, [this](const QString& upload_id, int n) {
        Q_UNUSED(upload_id)
        this->dlg_->progressBar->setVisible(false);
        UploadWorker* worker = qobject_cast<UploadWorker*>(QObject::sender());
        if (worker->success()) {
            ::history_set_upload_end(this->upload_info.history_id);
            this->dlg_->label->setText(QString("<h2>Upload finished!</h2>"
                                               "%1 file(s) uploaded, you can see them  "
                                               "<a href=\"https://dcm.cardiocare-project.eu/repo/patient/%2\">here</a>.")
                                       .arg(n)
                                       .arg(this->upload_info.patient_id));
            QAbstractButton* uploadBtn = this->dlg_->buttonBox->button(QDialogButtonBox::Apply);
            uploadBtn->setEnabled(false);
            this->dlg_->buttonBox->button(QDialogButtonBox::Cancel)->setText("OK");
        }
      });

    connect(w, &QDialog::finished, workerThread, &QThread::quit);
    connect(w, &QDialog::finished, &Worker::deleteLater);

    workerThread->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_action_About_triggered()
{
    QString title = "About DICOM Anonymizer Tool";
    QString text =
        "<h1>DICOM Anonymizer tool</h1>"
        "Version: " VERSION "<br>"
        "&copy; FORTH-ICS, 2024 <br><br>"
        "This tool uses the <a href='" CTP_URL "'>RSNA CTP anonymizer</a> and it is built "
        "with Qt under the <a href='https://www.qt.io/licensing/open-source-lgpl-obligations'>LGPLv3</a> license.";

    // QMessageBox::information(this, title, text);
    QMessageBox msgBox{QMessageBox::Information, title,text, QMessageBox::Ok, this};
    msgBox.setDetailedText(this->ctp_config());
    msgBox.exec();
}

QString MainWindow::ctp_config()
{
    if (this->ctp_config_ == "") {
        QStringList args;
        args << "-jar" << "DAT.jar";
        try {
            QString output = run_ctp(this, args);

            QStringList parts = output.split("Configuration:");
            if (parts.length() > 1)
                this->ctp_config_ = QString("CTP Configuration:%1").arg(parts.at(1));
        }
        catch (const ExecException&) {}
    }

    return this->ctp_config_;

}

void MainWindow::on_action_Open_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open DICOM folder"), "", QFileDialog::ShowDirsOnly);
    this->start_anonymize(dir);
}


void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

