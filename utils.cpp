#include "utils.h"
#include <QSettings>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

QString mdicom_path()
{
#ifdef Q_OS_WINDOWS
    QSettings s("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\mDicom.exe",
                QSettings::NativeFormat);
    return s.value("Default").toString();
#else
    return "";
#endif
}

ExecException::~ExecException() {}

QString run_ctp(QObject* caller, const QStringList& args)
{
    QDir appdir{QCoreApplication::applicationDirPath().append("/ctp")};

    qDebug().noquote() << "Running java in" << appdir<< "with cmd:" << args.join(" ");
    QProcess *proc = new QProcess(caller);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    proc->setWorkingDirectory(appdir.absolutePath());
    proc->setProcessEnvironment(env);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    proc->start("java", args, QIODevice::ReadOnly);

    if (!proc->waitForStarted(60000)) {
        throw ExecException(ExecException::DidntStart);
    }
    proc->waitForFinished(-1);
    QString output = proc->readAll().constData();
    qDebug().noquote() << output;

    if (proc->exitStatus() != QProcess::NormalExit || proc->exitCode() != 0) {
        throw ExecException(ExecException::CrashExit, output);

    }
    return output;
}

QString appDataDir()
{
    // Set these early in main():
    // QCoreApplication::setOrganizationName("YourOrg");
    // QCoreApplication::setApplicationName("YourApp");

    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

