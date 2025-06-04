#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>
#include <string>

class Worker : public QObject
{
    Q_OBJECT
private:
    const QString id_;
    const QString filePath_;
    const std::string site_id_;
    const std::string pid_prefix_;

    QString outFolder_;

public:
    Worker(const QString &filePath,
           const QString& site_id, const QString& pid_prefix);


    QString id() const { return this->id_; }

    QString temp_anon_folder() const {return this->outFolder_; }

private:

    class QEventLoop* eventLoop_;
public slots:
    void anonymize();

private:
    std::string hash_pid(const char* patient_id) const;
    void hash_clinical(const QString& csvInFilePath, const QString& csvOutFilePath, const QString& secret_key) const;

signals:
    void error(const QString& error);
    void finishedAnon(qint64 output_files_count, qint64 patient_count, qint64 input_files_count);
};

#endif // WORKER_H
