#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>
#include <unordered_map>

class Worker : public QObject
{
    Q_OBJECT
private:

    const QString filePath_;
    const QString patId_;
    const QString timePointId_;
    const QString timePointDescr_;
    const QString access_token_;

public:
    Worker(const QString &filePath, const QString& patId, const QString& timepointId,
           const QString& timepointDesc, const QString& token);
    
private:
    int zipFolder(const class QDir&, const QString&, const QString&);
    int upload_dcms(const class QDir&);

private:
    // Private counters for the upload progress

    qint64 totalBytes_; // the total size of the DICOMs to upload
    int nfiles_; // the total number of DICOMs to upload
    int nfinished_; // the total number of DICOMs finished uploading
    int nerror_; // the total number of DICOMs that failed to upload

    std::unordered_map<QString, qint64> uploaded_bytes_per_file_;

    class QEventLoop* eventLoop_;
public slots:
    void anonymizeAndUpload();
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void uploadFinished(class QNetworkReply*);

signals:
    void finished(int images_uploaded);
    void error(const QString& error);
    void progress(const QString&);
    void uploadProgress1000(int);
};

#endif // WORKER_H
