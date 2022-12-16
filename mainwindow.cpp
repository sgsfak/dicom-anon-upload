#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qdebug.h>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QDir>
#include <QThread>
#include <QTimer>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setAcceptDrops(true);
}


void MainWindow::on_tokens(const token_data& tokens) {
    this->tokens = tokens;
    QLabel *label = new QLabel(this);
    label->setText(tokens.access_token);
    this->statusBar()->addWidget(label);
    this->show();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    qDebug() << "dropEvent" << event->mimeData()->urls();
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

    qDebug() << "File:" << qf;
    QTimer::singleShot(100, this, [this, fileName]() {
        this->anonymize(fileName);
    });

}
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    qDebug() << mimeData->urls();
    if (mimeData->hasUrls() && mimeData->urls().constFirst().isLocalFile()) {
        this->statusBar()->showMessage(tr("Accepting.."));
        event->acceptProposedAction();
    }

}

static bool onMac()
{
#if defined(Q_OS_OSX)
    return true;
#else
    return false;
#endif
}


void MainWindow::anonymize(const QString &folder)
{
    QProgressDialog* pd = new QProgressDialog(this);
    pd->setLabelText(QObject::tr("Anonymizing .."));
    pd->setRange(0,0);
    pd->setCancelButton(nullptr);

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QStringList args;
    args << "-jar" << "DAT.jar"
        << "-n" << QString::number(qMin(4, QThread::idealThreadCount()))
        << "-da" << "anon.script"
        << "-in" << folder;
    qDebug().noquote() << "Running java with" << args;
    QProcess *proc = new QProcess(this);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
//    env.insert("JAVA_HOME", "/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home");


    proc->setWorkingDirectory(appdir.absolutePath());
    proc->setProcessEnvironment(env);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(proc, SIGNAL(finished(int)), pd, SLOT(cancel()));
    QObject::connect(proc, SIGNAL(error(QProcess::ProcessError)), pd, SLOT(cancel()));

    proc->start("java", args, QIODevice::ReadOnly);
    pd->exec();

    QString output = proc->readAll().constData();
    qDebug().noquote() << output;

}

MainWindow::~MainWindow()
{
    delete ui;
}
