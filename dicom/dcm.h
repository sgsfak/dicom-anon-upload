#pragma once

#include<QByteArray>
#include<QException>

class QFile;

namespace dcm {

QByteArray get_patient_id(QFile& dcm_file);

class ParseException : public QException
{
    QString reason_;
public:
    ParseException(const QString& reason): reason_(reason) {}
    void raise() const override { throw *this; }
    ParseException *clone() const override { return new ParseException(*this); }
    QString reason() const { return this->reason_; }
};
}

