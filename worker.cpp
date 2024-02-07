
#include "worker.h"

#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QIODevice>
#include <QCryptographicHash>
#include <QPair>

#define SERVER_URL "https://dcm.cardiocare-project.eu"

Worker::Worker(const QString &filePath, const QString& patId,
               const QString& timepointId, const QString& timepointDesc,
               const QString& token):

    id_(QDateTime::currentDateTime().toSecsSinceEpoch()),
    filePath_(filePath),
    patId_(patId),
    timePointId_(timepointId),
    timePointDescr_(timepointDesc),
    access_token_(token)

{}


QString Worker::temp_anon_folder() const
{
    QString sub_folfer = QString("/anon-out/%1_%2_%3")
            .arg(this->patId_)
            .arg(this->timePointId_)
            .arg(this->id_);
    return QCoreApplication::applicationDirPath().append(sub_folfer);
}

void Worker::anonymize() {

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QDir inputFolder{filePath_};

    QDateTime now = QDateTime::currentDateTime();

    QString outFolder = this->temp_anon_folder();

    int k = this->patId_.indexOf('-');
    if (k == -1) {
        k = 0;
    }
    QString siteId = "Cardiocare-" + this->patId_.left(k);

    // Use the Clinical trial attributes to pass the "time point" related annotation:
    // https://dicom.nema.org/medical/Dicom/2016b/output/chtml/part03/sect_C.7.2.3.html#sect_C.7.2.3.1.1
    QStringList args;
    args << "-jar" << "DAT.jar"
        << "-n" << QString::number(qMin(4, QThread::idealThreadCount()))
        << "-da" << "anon.script"
        << "-pSITEID" << siteId
        << "-pPATIENTID" << this->patId_
        << "-pTIMEPOINTID" << this->timePointId_
        << "-pTIMEPOINTDESCR" << this->timePointDescr_
        << "-in" << inputFolder.canonicalPath()
        << "-out" << outFolder;
    qDebug().noquote() << "Running java with" << args;
    QProcess *proc = new QProcess(this);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
//    env.insert("JAVA_HOME", "/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home");


    proc->setWorkingDirectory(appdir.absolutePath());
    proc->setProcessEnvironment(env);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    //    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
    //            this,
    //            [](int exitCode, QProcess::ExitStatus exitStatus)
    //    {
    //        qDebug() << "FINI" << exitCode << exitStatus;
    //    });

    proc->start("java", args, QIODevice::ReadOnly);

    if (!proc->waitForStarted(60000)) {
        emit error(QString("Could not start Java CTP command!"));
        return;
    }
    proc->waitForFinished(-1);
    if (proc->exitStatus() != QProcess::NormalExit) {
        emit error(QString("Java CTP command crashed!"));
        return;
    }

    QString output = proc->readAll().constData();
    qDebug().noquote() << output;

    if (proc->exitStatus() == QProcess::CrashExit || proc->exitCode() != 0) {
        emit error(QString("Anonymization through CTP failed!"));
        return;
    }
    emit finishedAnon();
}


