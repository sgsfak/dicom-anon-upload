#include "imph2mthread.h"
#include <QTcpSocket>
#include <QTextStream>
#include <QTextCodec>

ImpH2MThread::ImpH2MThread(QObject *parent) : QThread(parent), hostName("127.0.0.1"), port(27016), quit(false)
{
}
ImpH2MThread::~ImpH2MThread()
{
    stop();
    wait();
}

void ImpH2MThread::stop()
{
    mutex.lock();
    quit = true;
    mutex.unlock();
}

bool ImpH2MThread::is_stopped()
{
    bool q;
    mutex.lock();
    q = quit;
    mutex.unlock();
    return q;
}

void ImpH2MThread::requestNewCapture()
{
    if (!isRunning())
        start();
}

QString ImpH2MThread::send_command(const char* command, bool waitReply)
{
    QTcpSocket sock;
    sock.connectToHost(this->hostName, this->port);
    if (!sock.waitForConnected()) {
        emit error(sock.error(), sock.errorString());
        return "";
    }

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");

    sock.write(command);
    if (!sock.waitForBytesWritten()) {
        emit error(sock.error(), sock.errorString());
        return "";
    }

    if (!waitReply)
        return "";

    if (!sock.waitForReadyRead()) {
        emit error(sock.error(), sock.errorString());
        return "";
    }

    QStringList sl;
    char buf[1024];
    forever {
        qint64 lineLength = sock.readLine(buf, sizeof(buf));

        if (lineLength == -1) {
            break;
        }
        QString r = codec->toUnicode(buf).simplified();
        qDebug() << "SOCK ->" << buf  << "Simplified" << r;
        sl.append(r);
        if (!sock.waitForReadyRead()) {
            break;
        }
    }

    QString response = sl.join("\n");
    qDebug() << "SOCK: " << response;
    return response;
}

void ImpH2MThread::run()
{
    QString response = send_command("Launch capture");
    if (this->is_stopped() || response == "")
        return;
    if (response != "command received") {
        emit error(QAbstractSocket::SocketError::OperationError, "Error response from IMP H2M service! No selfie in the mobile?");
        return;
    }

    bool success = false;
    response = send_command("Matching result");
    for (uint i = 1; i < 10 && !this->is_stopped(); ++i) {
        if (response != "0") {
            qDebug() << "Got" << response;
            success = true;
            break;
        }
        QThread::sleep(5 < i ? 5 : i);
        if (this->is_stopped())
            break;
        response = send_command("Matching result");
    }
    send_command("Stop capture", false);

    if (this->is_stopped())
        return;

    QStringList sl = response.split("\n");
    if (success && sl.length()>1)
        emit credentials(sl.at(0), sl.at(1));
    else
        emit error(QAbstractSocket::SocketError::OperationError, "No credentials found!");

}
