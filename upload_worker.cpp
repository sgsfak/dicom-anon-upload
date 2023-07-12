#include "upload_worker.h"
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
#include <QFileInfo>
#include <QDirIterator>
#include <QList>
#include <QEventLoop>

#include <stdexcept>

#define SERVER_URL "https://dcm.cardiocare-project.eu"


UploadWorker::UploadWorker(const QString& token,  const QString& folder,
                           const QString& patId, const QString& timepointId):
    access_token_(token),
    folder_(folder),
    patient_id_(patId),
    timepoint_id_(timepointId),
    completed_(false),
    totalBytes_(0),
    nfiles_(0),
    nfinished_(0),
    nerror_(0)
{

}

namespace {

QString fileHash(const QString& fn, QCryptographicHash::Algorithm algo=QCryptographicHash::Md5)
{

    QFile f {fn};
    f.open(QIODevice::ReadOnly | QIODevice::ExistingOnly);
    QCryptographicHash hasher{algo};
    hasher.addData(&f);
    f.close();
    return hasher.result().toHex();
}

struct HttpException: public std::exception
{
    const int status_code;
    const QString status_description;

    HttpException(): status_code(0), status_description("") {}
    HttpException(int c, const QString& d): status_code(c), status_description(d) {}
};


QJsonDocument send_post_req(const QString& url, const QString& content_type, const QString& token, const QByteArray& content)
{
    qDebug().noquote() << " * POSTing to" << url << ":" << content;

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

    QJsonDocument response;

    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status_code.isValid()) {
        qDebug().noquote() << "* POST to" << url << "returned no valid status code";
        throw HttpException(0,
                            "Communication error with the server");
    }
    else if (status_code.toInt() / 100 != 2) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray();
        qDebug().noquote() << "* POST to" << url << "returned:" << status_code.toInt() << reason;
        throw HttpException(status_code.toInt(), reason);
    }
    else {
        QByteArray result = reply->readAll();
        response = QJsonDocument::fromJson(result);
    }
    return response;

}

}


int UploadWorker::upload_dcms(const QDir& outputAnonFolder)
{
    this->completed_ = false;

    QList<QFileInfo> dcm_list;
    QDirIterator iter(outputAnonFolder, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        QString s = iter.next();
        // qDebug().noquote() << "Now at " << s;
        QFileInfo fileInfo = iter.fileInfo();
        if (fileInfo.isFile()) {
            dcm_list.append(fileInfo);
        }
    }

    QVector<QPair<QFileInfo, QString> > file_hashes;
    qint64 total_bytes = 0;
    for(const QFileInfo& fi: dcm_list) {
        QString hash = ::fileHash(fi.absoluteFilePath());
        file_hashes.push_back(qMakePair(fi, hash));
        total_bytes += fi.size();
    }

    QJsonArray arr;
    for(auto&p: file_hashes) {
        QJsonObject obj;
        obj.insert("filename", p.first.fileName());
        obj.insert("size", p.first.size());
        obj.insert("md5_hash", p.second);
        arr.append(obj);
    }
    QJsonDocument response;
    try {
        QJsonObject obj;
        obj.insert("files", arr);
        obj.insert("patient_id", this->patient_id_);
        obj.insert("timepoint_id", this->timepoint_id_);
        QByteArray json_data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        response = ::send_post_req(SERVER_URL "/uploads",
                                   "application/json",
                                   this->access_token_,
                                   json_data);
    }
    catch (HttpException ex) {
        if (ex.status_code == 401) {
            emit error("Error authenticating with the server, you 'd better reopen the application!");
        }
        else if (ex.status_code == 0) {
            emit error("error communicating with the server");
        }
        else {
            emit error(ex.status_description);
        }
        return 0;
    }

    QString upload_id = response.object().value("id").toString();
    qDebug() << "upload id" << upload_id;

    this->nfiles_ = file_hashes.size();
    this->totalBytes_ = total_bytes;
    this->nfinished_ = this->nerror_ = 0;

    // Send all of them using the same QNetworkAccessManager
    this->eventLoop_ = new QEventLoop(this);
    QNetworkAccessManager manager;

    QObject::connect(&manager, &QNetworkAccessManager::finished, this, &UploadWorker::uploadFinished);

    QPair<QFileInfo, QString> p;
    for(auto& p: file_hashes) {
        QNetworkRequest request;
        request.setUrl(QString(SERVER_URL "/dicom-upload?id=%1").arg(upload_id));
        request.setRawHeader("Content-MD5", p.second.toUtf8());
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
        QObject::connect(reply, &QNetworkReply::uploadProgress, this, &UploadWorker::uploadProgress);
    }
    this->eventLoop_->exec();
    if (this->nerror_ > 0) {
        emit error(QString("Error: %1 files failed to be uploaded, better contact admin").arg(this->nerror_));
        return 0;
    }

    try {
        QJsonObject obj;
        obj.insert("status", "finished");
        QByteArray json_data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        response = ::send_post_req(QString(SERVER_URL "/uploads/%1").arg(upload_id),
                                   "application/json",
                                   this->access_token_,
                                   json_data);
    }
    catch (const HttpException& ex) {
        if (ex.status_code == 401) {
            emit error("Error authenticating with the server, you 'd better reopen the application!");
        }
        else if (ex.status_code == 0) {
            emit error("Error communicating with the server");
        }
        else {
            emit error("Server error:" + ex.status_description);
        }
        return 0;
    }
    this->completed_ = true;
    return this->nfiles_;
}


void UploadWorker::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (bytesTotal == 0)
        return;

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
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



void UploadWorker::uploadFinished(QNetworkReply* reply)
{

    this->nfinished_ += 1;
    if (reply->error() != QNetworkReply::NoError) {
        qDebug().noquote() << "* Upload to" << reply->url() << "returned" << reply->error();
        this->nerror_ += 1;
    }

    reply->deleteLater();

    if (this->nfinished_ == this->nfiles_) {
        this->eventLoop_->quit();
    }

}

void UploadWorker::upload()
{

    QDir outFolder = this->folder();

    int n = this->upload_dcms(outFolder);
    if (this->success()) {
//        QDir(outFolder).removeRecursively();
        emit finished(n);
    }
}

