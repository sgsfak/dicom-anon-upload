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
#include <QUuid>

namespace {
const char* const UIDROOT="1.3.6.1.4.1.58108.2023";
}

/* It reads recursively any file in the given dicomFolder, tries to parse
 * each file found, and returns a hash map from PatientIDs to the list of DICOM file names
 * that contain it.
 */
static QHash<QString, QList<QString>> dcms_pids(const QString& dicomFolder)
{

    QHash<QString, QList<QString>> dcm_pids_found;

    QDirIterator it(dicomFolder, QStringList(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {

        QFile f {it.next()};
        try {
            auto dcm_info = dcm::get_file_info(f);
            // if (!dcm_pids_found.contains(dcm_info.patient_id))
            //     qDebug().noquote() << dcm_info;
            dcm_pids_found[dcm_info.patient_id].append(f.fileName());
        } catch (const dcm::ParseException& e) {
            qDebug().noquote() << "DICOM ParseException for file"
                               << f.fileName() << ":" << e.reason();
        }
    }
    return dcm_pids_found;
}

Worker::Worker(const QString &filePath,
               const QString& site_id, const QString& pid_prefix):

    id_(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddThhmmss")+
        QString("_%1").arg(QRandomGenerator::global()->bounded(1000), 3, 10, QChar('0'))),
    filePath_(filePath),
    site_id_(site_id.toUtf8().constData()),
    pid_prefix_(pid_prefix.toUtf8().constData())

{

    QDir appDir{appDataDir()};
    QString run_folder = QString("RUNS/RUN_%1").arg(this->id_);
    appDir.mkpath(run_folder);
    this->outFolder_ = appDir.filePath(run_folder);
}


void Worker::anonymize() {

    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};
    qDebug() << "appdir=" << appdir;

    QDir inputFolder{filePath_};


    // QDateTime now = QDateTime::currentDateTime();

    QString outFolder = this->temp_anon_folder();
    QDir outDir{outFolder};

    // QString siteId = QString::fromUtf8(this->site_id_);

    // To make more difficult the identification of the original provider given
    // the contents of the anonymized DICOM files, we hash the "site id" and add
    // its hex digest as the "provider id" in the result DICOM images. We are using
    // SHA-256 which produces hex string of 32 x 2 = 64 bytes, so it's ok to add it
    // on any tag of "LO" (Long String) value representation (VR) that is at most 64
    // characters/bytes according to DICOM :
    // https://dicom.nema.org/dicom/2013/output/chtml/part05/sect_6.2.html#:~:text=LO
    //
    QByteArray hexHash = QCryptographicHash::hash(this->site_id_.c_str(), QCryptographicHash::Sha256).toHex();
    QString providerId = QString::fromLatin1(hexHash);

    QString pepper = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QStringList csvFilters;
    csvFilters << "*.csv";
    const QFileInfoList csvList = inputFolder.entryInfoList(csvFilters);
    for (const auto& csvFile: csvList) {

        // QFileInfo csvFile = csvList[0];
        QString fileName = csvFile.fileName();
        // Ignore any CSVs starting with _
        if (fileName.startsWith("_")) {
            continue;
        }

        try {
            this->hash_clinical(csvFile.absoluteFilePath(), outDir.filePath(fileName), pepper);
        }
        catch (const QException& e) {
            emit error(QString("Error: %1").arg(e.what()));
        }
    }


    QStringList args;
    args << "-jar" << "DAT.jar"
         << "-n" << QString::number(qMin(4, QThread::idealThreadCount()))
         << "-da" << "anon.script"
         << "-pPROVIDERID" << providerId
         << "-pSECRET_KEY" << pepper
         << "-pUIDROOT" << ::UIDROOT
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
    qDebug().noquote() << "CTP process finished!";
    const auto pids_dcms = ::dcms_pids(outFolder);
    qint64 pids_count = pids_dcms.count();
    qint64 files_count = 0;
    for(const auto & v: pids_dcms) {
        files_count += v.count();
    }

    qDebug().noquote() << "output folder contains" << files_count << "anon. DICOM files";

    const auto input_pids_dcms = ::dcms_pids(this->filePath_);
    qint64 input_files_count = 0;
    for(const auto & v: input_pids_dcms) {
        input_files_count += v.count();
    }

    qDebug().noquote() << files_count << " output files read, output DICOM patients:" << pids_count;
    emit finishedAnon(files_count, pids_count, input_files_count);
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

void Worker::hash_clinical(const QString& inFile, const QString& outFile, const QString& secret_key) const
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
    std::string pp = "[" + secret_key.toStdString() + "]";

    // Get the column names in the "header row" and write them as first row
    // Note that we assume that we have a Header row!! If not, then
    // the first input row will be ignored and we will not map its Patient ID!!
    std::vector<std::string> cols = reader.get_col_names();
    if (!cols.empty()) {
        writer << cols;
    }

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


