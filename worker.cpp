
#include "worker.h"

#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QThread>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>

#include "zip.h"

Worker::Worker(const QString &filePath, const QString& patId, const QString& label, const QString& token):
    filePath_(filePath),
    patId_(patId),
    label_(label),
    access_token_(token)
{}

void Worker::anonymizeAndUpload() {

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QDir inputFolder{filePath_};

    QDateTime now = QDateTime::currentDateTime();

    QDir outputAnonFolder{ QCoreApplication::applicationDirPath().append("/anon-out/") };
    QString tempDir = QString::number(now.toSecsSinceEpoch()) + "-" + inputFolder.dirName();
    QString outFolder = outputAnonFolder.filePath(tempDir );

    QStringList args;
    args << "-jar" << "DAT.jar"
        << "-n" << QString::number(qMin(4, QThread::idealThreadCount()))
        << "-da" << "anon.script"
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
    emit progress("Anonymizing..");
    if (!proc->waitForStarted()) {
        emit error(QString("Could not start Java CTP command!"));
        return;
    }
    if (!proc->waitForFinished()) {
        emit error(QString("Could not run Java CTP command!"));
        return;
    }

    QString output = proc->readAll().constData();
    qDebug().noquote() << output;

    if (proc->exitStatus() == QProcess::CrashExit || proc->exitCode() != 0) {
        emit error(QString("Anonymization through CTP failed!"));
        return;
    }


    emit progress("Creating ZIP with images..");

    QString outZip = outputAnonFolder.filePath(tempDir + ".zip");

    zip_t* z = zip_open(outZip.toLocal8Bit().data(), ZIP_TRUNCATE|ZIP_CREATE, nullptr);

    QDir outputFolder{ outFolder };
    QDirIterator it(outputFolder.canonicalPath(), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fn = it.next();
        QFileInfo fi = it.fileInfo();
        QString entry = outputAnonFolder.relativeFilePath(fn);
        if (fi.isDir()) {
            zip_dir_add(z, entry.toLocal8Bit().data(), ZIP_FL_ENC_UTF_8);
        }
        else {
            zip_source_t* t = zip_source_file(z, fn.toLocal8Bit().data(), 0, 0);
            zip_add(z, entry.toLocal8Bit().data(), t);
        }
        qDebug().noquote() << "Adding" << ( fi.isDir() ? "dir" : "file") << entry;
    }
    qDebug().noquote() << "Finished adding" ;
    emit progress("Creating final ZIP file..");
    zip_close(z);


    // Upload the Zip:

    QFile zipFile{outZip};
    if (!zipFile.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        emit error("Zip file was not created!");
        return;
    }


    emit progress("Uploading DICOM images..");

    QEventLoop synchronous;
    QNetworkAccessManager manager;

    connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    QUrl uploadEndpoint {"https://dcm.cardiocare-project.eu/dicom/instances"};
    QNetworkRequest request(uploadEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/zip");
    QString authHeader = QString("Bearer ") + this->access_token_;
    request.setRawHeader("Authorization", authHeader.toUtf8());

    QNetworkReply* reply = manager.post(request, &zipFile);
    synchronous.exec();

    zipFile.close();
    reply->deleteLater();

    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status_code.isValid()) {
        emit error(QString("Communication error: %1").arg(reply->errorString()));
        return;
    }
    if (status_code.toInt() / 100 != 2) {
        emit error(QString("Server error code: %1").arg(status_code.toInt()));
        return;
    }

    QByteArray result = reply->readAll();
    QJsonArray jsonArr = QJsonDocument::fromJson(result).array();
    int n = jsonArr.size();
    qDebug().noquote() << "Uploaded " << n << "images";


    emit finished(n);
}

