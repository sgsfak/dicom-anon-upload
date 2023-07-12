#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>
#include <unordered_map>

class Worker : public QObject
{
    Q_OBJECT
private:
    const qint64 id_;
    const QString filePath_;
    const QString patId_;
    const QString timePointId_;
    const QString timePointDescr_;
    const QString access_token_;

public:
    Worker(const QString &filePath, const QString& patId, const QString& timepointId,
           const QString& timepointDesc, const QString& token);


    QString patient_id() const { return this->patId_; }
    QString timepoint_id() const { return this->timePointId_; }
    QString timepoint() const { return this->timePointDescr_; }

    QString temp_anon_folder() const;

private:

    class QEventLoop* eventLoop_;
public slots:
    void anonymize();

signals:
    void error(const QString& error);
    void finishedAnon();
};

#endif // WORKER_H
