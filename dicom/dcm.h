#pragma once

#include<QByteArray>
#include<QException>

class QDebug;


class QFile;

namespace dcm {

struct DcmFileInfo {
    QString patient_id;
    QString study_uid;
    QString series_uid;
    QString series_description;
};

QDebug &operator<<(QDebug &, const DcmFileInfo& f);

DcmFileInfo get_file_info(QFile& dcm_file);

class ParseException : public QException
{
    QString reason_;
public:
    explicit ParseException(const QString& reason): reason_(reason) {}
    ParseException(const ParseException& other): reason_(other.reason_) {}
    void raise() const override { throw *this; }
    ParseException *clone() const override { return new ParseException(*this); }
    QString reason() const { return this->reason_; }
    virtual ~ParseException() override;
};
}

