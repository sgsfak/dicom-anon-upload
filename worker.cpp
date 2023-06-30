
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
#include <QCryptographicHash>
#include <QBuffer>
#include <QDataStream>
#include <QVector>
#include <QPair>

#include "zip.h"
#include "xxhash/xxh_x86dispatch.h"

Worker::Worker(const QString &filePath, const QString& patId,
               const QString& timepointId, const QString& timepointDesc,
               const QString& token):
    filePath_(filePath),
    patId_(patId),
    timePointId_(timepointId),
    timePointDescr_(timepointDesc),
    access_token_(token),
    totalBytes_(0),
    nfiles_(0),
    nfinished_(0),
    nerror_(0)

{}

namespace {

QString fileHash(const QString& fn, QCryptographicHash::Algorithm algo=QCryptographicHash::Sha256)
{

    QFile f {fn};
    f.open(QIODevice::ReadOnly | QIODevice::ExistingOnly);
    QCryptographicHash sha{algo};
    sha.addData(&f);
    f.close();
    return sha.result().toHex();
}

QString fileHash_xxh3(const QString& fn)
{

    QFile f {fn};
    f.open(QIODevice::ReadOnly | QIODevice::ExistingOnly);
    uchar* p = f.map(qint64(0), f.size());
    quint64 h = XXH3_64bits_dispatch(p, static_cast<size_t>(f.size()));
    f.unmap(p);
    f.close();

    QString hexvalue = QString("%1").arg(h, 8, 16, QLatin1Char( '0' ));
    // qDebug().noquote() << "File:" << fn << ":" << hexvalue;
    return hexvalue;

    // Alternative:
    //    QBuffer buf;
    //    buf.open(QBuffer::ReadWrite);
    //    QDataStream stream(&buf);

    //    stream << h;
    //    return buf.buffer().toHex();

    // See also https://github.com/Cyan4973/xxHash/issues/829 :
    /*
        std::string xxh128_hash_to_string(XXH128_hash_t hash) {
             char buf[33];
             snprintf(buf, sizeof(buf),
                      "%016llx%016llx",
                      (unsigned long long)hash.high64,
                      (unsigned long long)hash.low64);
             return std::string(buf);
         }
     */
}

int send_post_req(const QString& url, const QString& content_type, const QString& token, const QByteArray& content,
                          QJsonObject& response) {

    QNetworkAccessManager manager;

    QEventLoop eventloop;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventloop, SLOT(quit()));

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,content_type);
    QString authHeader = QString("Bearer ") + token;
    request.setRawHeader("Authorization", authHeader.toUtf8());

    QNetworkReply* reply = manager.post(request, content);
    eventloop.exec();

    reply->deleteLater();

    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status_code.isValid()) {
        return -1;
    }
    if (status_code.toInt() / 100 != 2) {
        return -2;
    }

    QByteArray result = reply->readAll();
    response = QJsonDocument::fromJson(result).object();
    return 0;

}
}

void Worker::uploadFinished(QNetworkReply* reply)
{

    this->nfinished_ += 1;
    if (reply->error() != QNetworkReply::NoError) {
        this->nerror_ += 1;
    }
    /*
    else {

        QByteArray result = reply->readAll();
        qDebug().noquote() << "GOT" << result;
    }
    */

    reply->deleteLater();

    if (this->nfinished_ == this->nfiles_) {
        this->eventLoop_->quit();
    }

}

int Worker::upload_dcms(const QDir& outputAnonFolder)
{
    QList<QFileInfo> dcm_list;
    QDirIterator iter(outputAnonFolder, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        QString s = iter.next();
        qDebug().noquote() << "Now at " << s;
        QFileInfo fileInfo = iter.fileInfo();
        if (fileInfo.isFile()) {
            dcm_list.append(fileInfo);
        }
    }

    QVector<QPair<QFileInfo, QString> > file_hashes;
    QFileInfo fi;
    qint64 total_bytes = 0;
    Q_FOREACH(fi, dcm_list) {
        QString hash = ::fileHash_xxh3(fi.absoluteFilePath());
        file_hashes.push_back(qMakePair(fi, hash));
        total_bytes += fi.size();
    }

    QJsonObject response;
    int k = ::send_post_req("https://dcm.cardiocare-project.eu/uploads", "application/json",
                            this->access_token_, QByteArray{}, response);
    if (k < 0) {
        emit error("error communicating with the server");
        return 0;
    }

    QString upload_id = response.value("id").toString();
    qDebug() << "upload id" << upload_id;

    this->nfiles_ = file_hashes.size();
    this->totalBytes_ = total_bytes;
    this->nfinished_ = this->nerror_ = 0;

    // Send all of them using the same QNetworkAccessManager
    this->eventLoop_ = new QEventLoop(this);
    QNetworkAccessManager manager;

    QObject::connect(&manager, &QNetworkAccessManager::finished, this, &Worker::uploadFinished);

    emit progress("Uploading..");

    QPair<QFileInfo, QString> p;
    Q_FOREACH(p, file_hashes) {
        QNetworkRequest request;
        request.setUrl(QString("https://dcm.cardiocare-project.eu/dicom-upload?id=%1").arg(upload_id));
        request.setRawHeader("Content-XXH3", p.second.toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/dicom");
        QString authHeader = QString("Bearer ") + this->access_token_;
        request.setRawHeader("Authorization", authHeader.toUtf8());

        QFile* data = new QFile( p.first.absoluteFilePath() );
        if (!data->open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
            emit error("Can't open DICOM file: " + p.first.fileName());
            return 0; // XXX
        }

        QNetworkReply* reply = manager.post(request, data);
        data->setParent(reply); // The QFile will be closed when reply is deleted

        // Get updates on the upload progress of this file:
        QObject::connect(reply, &QNetworkReply::uploadProgress, this, &Worker::uploadProgress);
    }
    this->eventLoop_->exec();
    return this->nfiles_;
}


void Worker::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (bytesTotal == 0)
        return;

    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(QObject::sender());
    QFile* f = reply->findChild<QFile*>();

    uploaded_bytes_per_file_[f->fileName()] = bytesSent;


    qint64 uploadedBytes = 0; // the total size of the DICOMs uploaded so far
    for (auto p = uploaded_bytes_per_file_.begin(); p != uploaded_bytes_per_file_.end(); p++) {
        uploadedBytes += p->second;
    }

    int k = uploadedBytes == this->totalBytes_ ? 1000 : int(uploadedBytes * 1000/this->totalBytes_);
//    qDebug().noquote() << f->fileName() << bytesSent << bytesTotal << "Total" << uploadedBytes << this->totalBytes_ << k;
    emit uploadProgress1000(k);
}

int Worker::zipFolder(const QDir& outputAnonFolder, const QString& outZip, const QString& outFolder)
{
    zip_t* z = zip_open(outZip.toLocal8Bit().data(), ZIP_TRUNCATE|ZIP_CREATE, nullptr);

    QDir outputFolder{ outFolder };
    QDirIterator it(outputFolder.canonicalPath(), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int nfiles = 0;
    while (it.hasNext()) {
        QString fn = it.next();
        QFileInfo fi = it.fileInfo();
        QString entry = outputAnonFolder.relativeFilePath(fn);
        if (fi.isDir()) {
            zip_dir_add(z, entry.toLocal8Bit().data(), ZIP_FL_ENC_UTF_8);
        }
        else {
            qDebug().noquote() << fi.fileName() << ::fileHash_xxh3(fi.absoluteFilePath());

            zip_source_t* t = zip_source_file(z, fn.toLocal8Bit().data(), 0, 0);
            zip_add(z, entry.toLocal8Bit().data(), t);
            nfiles++;
        }
        qDebug().noquote() << "Adding" << ( fi.isDir() ? "dir" : "file") << entry;
    }
    qDebug().noquote() << "Finished adding, files added:" << nfiles;
    emit progress("Creating final ZIP file..");
    zip_close(z);
    return nfiles;
}

void Worker::anonymizeAndUpload() {

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QDir inputFolder{filePath_};

    QDateTime now = QDateTime::currentDateTime();

    QDir outputAnonFolder{ QCoreApplication::applicationDirPath().append("/anon-out/") };
    QString tempDir = QString::number(now.toSecsSinceEpoch()) + "-" + inputFolder.dirName();
    QString outFolder = outputAnonFolder.filePath(tempDir );

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

    int n = this->upload_dcms(outFolder);

    /*
    emit progress("Creating ZIP with images..");

    QString outZip = outputAnonFolder.filePath(tempDir + ".zip");
    int nfiles = zipFolder(outputAnonFolder, outZip, outFolder);
    if (nfiles == 0) {
        qDebug().noquote() << "No files added!";
        emit error("No DICOM files found!");
        return;
    }

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

    QUrl uploadEndpoint {"https://dcm.cardiocare-project.eu/instances"};
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

    */
    emit finished(n);
}

