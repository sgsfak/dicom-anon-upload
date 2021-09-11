#ifndef IMPH2MTHREAD_H
#define IMPH2MTHREAD_H

#include <QMutex>
#include <QThread>

class ImpH2MThread : public QThread
{
    Q_OBJECT
public:
    explicit ImpH2MThread(QObject *parent = nullptr);
    virtual ~ImpH2MThread() override;

    void requestNewCapture();
    void run() override;
    void stop();
    bool is_stopped();

signals:
    void credentials(const QString &username, const QString& password);
    void error(int socketError, const QString &message);

private:
    QString hostName;
    quint16 port;
    QMutex mutex;
    bool quit;

    QString send_command(const char* command, bool waitReply=true);
};

#endif // IMPH2MTHREAD_H
