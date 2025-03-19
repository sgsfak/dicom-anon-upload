#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "worker.h"
#include "utils.h"
#include "utilities/csv.hpp"
#include "utilities/bigint.hpp"
#include "dicom/dcm.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QIODevice>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QPair>


Worker::Worker(const QString &filePath,
               const QString& site_id, const QString& pid_prefix):

    id_(QDateTime::currentDateTimeUtc().toString("yyyyMMddThhmmss")+
        QString("_%1").arg(QRandomGenerator::global()->bounded(1000), 3, 10, QChar('0'))),
    filePath_(filePath),
    site_id_(site_id.toUtf8().constData()),
    pid_prefix_(pid_prefix.toUtf8().constData())

{

    auto tempDir = QDir::temp();
    QString sub_folfer = QString("anon_job_%1").arg(this->id_);
    tempDir.mkdir(sub_folfer);
    this->outFolder_ = tempDir.filePath(sub_folfer);

}


void Worker::anonymize() {

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QDir inputFolder{filePath_};


    // QDateTime now = QDateTime::currentDateTime();

    QString outFolder = this->temp_anon_folder();
    QDir outDir{outFolder};

    QStringList csvFilters;
    csvFilters << "*.csv";
    QFileInfoList csvList = inputFolder.entryInfoList(csvFilters);
    if (!csvList.empty()) {
        QFileInfo csvFile = csvList[0];
        QString fileName = csvFile.fileName();

        try {
            this->hash_clinical(csvFile.absoluteFilePath(), outDir.filePath(fileName));
        }
        catch (const QException& e) {
            emit error(QString("Error: %1").arg(e.what()));
        }
    }


    QString siteId = QString::fromUtf8(this->site_id_);

    QStringList args;
    args << "-jar" << "DAT.jar"
        << "-n" << QString::number(qMin(4, QThread::idealThreadCount()))
        << "-da" << "anon.script"
        << "-pSITEID" << siteId
        << "-in" << inputFolder.canonicalPath()
        << "-out" << outFolder;

    try {
        run_ctp(this, args);
    }
    catch (const ExecException& ex) {
        if (ex.status_ == ExecException::DidntStart) {
            emit error(QString("Could not start Java CTP command! Are you sure you have Java installed?"));
        }
        else if (ex.status_ == ExecException::CrashExit) {
            emit error(QString("Anonymization through CTP failed! Log:\n\n%1").arg(ex.output_));

        }
        return;

    }
    QSet<std::string> pids;
    QDirIterator it(outFolder, QStringList(), QDir::Files, QDirIterator::Subdirectories);
    qsizetype cnt = 0;
    while (it.hasNext()) {

        QFile f {it.next()};
        try {
            auto patient_id = dcm::get_patient_id(f);
            pids.insert(patient_id.constData());
            // qDebug().noquote() << "File" << f.fileName() << ", Patient ID=" << patient_id.constData();
        }
        catch(const dcm::ParseException& e) {

            qDebug().noquote() << "DICOM ParseException for file" << f.fileName() << ":" << e.reason();
        }
        cnt += 1;
    }
    qDebug().noquote() << cnt << "files read, output DICOM patients:" << pids.count();
    emit finishedAnon(cnt, pids.count());
}


std::string Worker::hash_pid(const char* patient_id) const
{
    QCryptographicHash h{QCryptographicHash::Md5};
    QByteArray bpid(patient_id);
    h.addData(bpid);
    QByteArray ba = h.result();
    bigint bi;
    for (qsizetype i = 0; i < ba.size(); ++i) {
        int c = static_cast<unsigned char>(ba.at(i));
        bi *= 256;
        bi += c;
    }

    std::ostringstream iss;
    iss << this->pid_prefix_;
    iss << "-";
    iss << bi;
    return iss.str();
}

namespace {
    class InvalidFileException: public QException
    {
    private:
        std::string what_;
    public:
        InvalidFileException(const QString& filename);

        virtual const char* what() const noexcept override {
            return this->what_.c_str();
        }
    };
}

InvalidFileException::InvalidFileException(const QString& filename) {
    this->what_ = "Failed to open file: ";
    this->what_ += filename.toStdString();
}

void Worker::hash_clinical(const QString& inFile, const QString& outFile) const
{
    // qDebug() << "I am reading from"<<inFile << "and write to" << outFile;

    if (!QFileInfo::exists(inFile)) {
        throw InvalidFileException(inFile);
    }

    csv::CSVReader reader{inFile.toStdString()};
    std::ofstream ostrm { outFile.toStdString(), ostrm.out | ostrm.trunc};

    if (!ostrm.is_open()) {
        throw InvalidFileException(outFile);
    }

    auto writer = csv::make_csv_writer(ostrm);
    csv::CSVRow row;
    std::string pp = "[" + this->site_id_ + "]";

    while (reader.read_row(row)) {
        std::vector<std::string> out_row;
        for (auto f: row) {
            out_row.push_back(f.get());
        }


        std::string pid = row[0].get();
        std::string toHash =  pp + pid;
        std::string hashedPid = this->hash_pid(toHash.c_str());
        // qDebug() << "IN PID"<< pid << "OUT" << hashedPid;
        out_row[0] = hashedPid;

        writer << out_row;
    }
    ostrm.flush();
}


