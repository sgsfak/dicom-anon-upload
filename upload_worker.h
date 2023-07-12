#ifndef UPLOAD_WORKER_H
#define UPLOAD_WORKER_H

#include <QObject>
#include <QString>
#include <unordered_map>

class UploadWorker : public QObject
{
    Q_OBJECT
public:
    explicit UploadWorker(const QString& token, const QString& folder,
                          const QString& patId, const QString& timepointId);

    bool success() const { return this->completed_ && this->nerror_ == 0; }

    QString folder() const { return this->folder_; }

private:
    int upload_dcms(const class QDir&);

private:
    const QString access_token_;
    const QString folder_;
    const QString patient_id_;
    const QString timepoint_id_;

    bool completed_;

    // Private counters for the upload progress

    qint64 totalBytes_; // the total size of the DICOMs to upload
    int nfiles_; // the total number of DICOMs to upload
    int nfinished_; // the total number of DICOMs finished uploading
    int nerror_; // the total number of DICOMs that failed to upload

    std::unordered_map<QString, qint64> uploaded_bytes_per_file_;

    class QEventLoop* eventLoop_;
public slots:
    void upload();

    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void uploadFinished(class QNetworkReply*);

signals:
    void finished(int images_uploaded);
    void error(const QString& error);

    void uploadProgress1000(int);

};

#endif // UPLOAD_WORKER_H
