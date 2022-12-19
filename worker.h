#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>

class Worker : public QObject
{
    Q_OBJECT
private:

    const QString filePath_;
    const QString patId_;
    const QString label_;
    const QString access_token_;

public:
    Worker(const QString &filePath, const QString& patId, const QString& label, const QString& token);

public slots:
    void anonymizeAndUpload();

signals:
    void finished(int images_uploaded);
    void error(const QString& error);
    void progress(const QString&);
};

#endif // WORKER_H
