#ifndef UTILS_H
#define UTILS_H
#include <QString>
#include <QException>
#include <QProcess>

QString mdicom_path();

class ExecException : public QException
{
public:
    enum ExecStatus {
         NormalExit,
         CrashExit,
         DidntStart
    };

    ExecException(ExecStatus status): status_(status) {}
    ExecException(ExecStatus status, const QString& output): output_(output), status_(status)
    {}
    ExecException(const ExecException& other): output_(other.output_), status_(other.status_)
    {}

    void raise() const override { throw *this; }
    ExecException *clone() const override { return new ExecException(*this); }
    virtual ~ExecException() override;

public:
    QString output_;
    ExecStatus status_;
};


QString run_ctp(QObject* caller, const QStringList& args);
QString appDataDir();

#endif // UTILS_H
