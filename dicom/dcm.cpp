#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dcm.h"
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QTextCodec>

#define DICOM_MAGIC "DICM"
#define DICOM_PREAMBLE_SIZE 128
#define GROUP_0002 0x0002
#define TRANSFER_SYNTAX_ELEMENT 0x0010
#define UNDEFINED_LENGTH 0xFFFFFFFF

// #define DEBUG

#ifdef DEBUG
#define PRINT_ERR(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT_ERR(...)
#endif

namespace dcm {
QDebug &operator<<(QDebug & q, const DcmFileInfo& f)
{
    QString a =
        QString("('%1', '%2', '%3', \"%4\")")
            .arg(f.patient_id, f.study_uid, f.series_uid, f.series_description);
    q << a;
    return q;
}
}


// DICOM Tag structure
struct DICOMTag
{
    quint16 group;
    quint16 element;
    quint32 length;
    char vr[4];

    DICOMTag() : group(0), element(0), length(UNDEFINED_LENGTH), vr{} {}

    QString toString() {
        return QString( "(%1,%2)").arg(QString::number(this->group, 16), 4, '0').arg(QString::number(this->element, 16), 4, '0');
    }
};


#ifdef DEBUG
void print_tag(DICOMTag tag)
{
    qDebug().noquote().nospace()
        << tag.toString() << "[" << tag.vr << "]" << "{" << tag.length << "}";
}
#else
void print_tag(DICOMTag) {}
#endif

// Function to manually assemble a 16-bit integer from little-endian bytes
static quint16 read_uint16(QDataStream& dicom_stream)
{
    quint16 u;
    dicom_stream >> u;
    if (dicom_stream.status() != QDataStream::Ok) {
        throw dcm::ParseException("Could not read UINT16");
    }
    return u;
}
static quint32 read_uint32(QDataStream& dicom_stream)
{

    quint32 u;
    dicom_stream >> u;
    if (dicom_stream.status() != QDataStream::Ok) {
        throw dcm::ParseException("Could not read UINT32");
    }
    return u;
}

// Function to parse and read DICOM tags
// actually ony the tag element, VR (optional, based on the explicit param)
// and the length are read
static DICOMTag read_tag(QDataStream& dicom_stream, int explicit_vr)
{
    DICOMTag tag;

    tag.group = read_uint16(dicom_stream) ;
    tag.element = read_uint16(dicom_stream);


    /* 
      According to https://dicom.nema.org/dicom/2013/output/chtml/part05/sect_7.5.html :

     There are three special SQ related Data Elements that are not ruled by the VR encoding
     rules conveyed by the Transfer Syntax. They shall be encoded as Implicit VR. 
     These special Data Elements are Item (FFFE,E000), Item Delimitation Item (FFFE,E00D), 
     and Sequence Delimitation Item (FFFE,E0DD). However, the Data Set within the Value Field 
     of the Data Element Item (FFFE,E000) shall be encoded according to the rules conveyed by
     the Transfer Syntax.
    */
    if (!explicit_vr || (tag.group == 0xFFFE && tag.element == 0xE000) ||
        (tag.group == 0xFFFE && tag.element == 0xE00D) ||
        (tag.group == 0xFFFE && tag.element == 0xE0DD)) {
        tag.length = read_uint32(dicom_stream);
        return tag;
    }
    // OK, now we are in explicit mode, so we read the VR

    if (dicom_stream.readRawData(tag.vr, 2) != 2) {
        throw dcm::ParseException("Tag "+tag.toString() + " has no VR info");
    }
    if (!(tag.vr[0] >='A' && tag.vr[0] <='Z' && tag.vr[1] >='A' && tag.vr[1] <='Z')) {
        tag.vr[0] = 'n';
        tag.vr[1] = 'a';
    }
    // https://dicom.nema.org/dicom/2013/output/chtml/part05/chapter_7.html#sect_7.1
    if ((tag.vr[0] == 'O' && tag.vr[1] == 'B') || // Case OB
        (tag.vr[0] == 'O' && tag.vr[1] == 'W') || // Case OW
        (tag.vr[0] == 'O' && tag.vr[1] == 'F') || // Case OF
        (tag.vr[0] == 'S' && tag.vr[1] == 'Q') || // Case SQ
        (tag.vr[0] == 'U' && tag.vr[1] == 'T') || // Case UT
        (tag.vr[0] == 'U' && tag.vr[1] == 'N')) { // Case UN
        read_uint16(dicom_stream); // Reserved, it should be 0x0000, so IGNORE THIS
        tag.length = read_uint32(dicom_stream);
    } else {
        tag.length = read_uint16(dicom_stream);
    }

    return tag;
}

static quint32 read_group_0002_length(QDataStream& dicom_stream)
{

    DICOMTag tag = read_tag(dicom_stream, 1);
    print_tag(tag);
    assert(tag.group == GROUP_0002 && tag.element == 0 && tag.length == 4);

    quint32 header_length = read_uint32(dicom_stream);
    return header_length;
}

dcm::ParseException::~ParseException() {}

namespace {

QTextDecoder* default_decoder() {
    const char* const default_encoding = "ISO8859-1";
    QTextDecoder* decoder =
        QTextCodec::codecForName(default_encoding)->makeDecoder();
    return decoder;
}
QString decode_string(const QByteArray& value, const QStringList& encodings) {
    // - https://dicom.innolitics.com/ciods/rt-radiation-set/sop-common/00080005
    // - https://github.com/pydicom/pydicom/blob/main/src/pydicom/charset.py#L25

    const char* const default_encoding = "ISO8859-1";

    static QHash<QString, const char*> encoding_map = {
        // default character set for DICOM
        {"", default_encoding},
        // alias for latin_1 too (iso_ir_6 exists as an alias to 'ascii')
        {"ISO_IR 6", default_encoding},
        {"ISO_IR 13", "Shift_JIS"},
        {"ISO_IR 100", default_encoding},
        {"ISO_IR 101", "ISO-8859-2"},
        {"ISO_IR 109", "ISO-8859-3"},
        {"ISO_IR 110", "ISO-8859-4"},
        {"ISO_IR 126", "ISO-8859-7"},        // Greek
        {"ISO_IR 127", "ISO-8859-6"},        // Arabic
        {"ISO_IR 138", "ISO-8859-8"},        // Hebrew
        {"ISO_IR 144", "ISO-8859-5"},        // Russian
        {"ISO_IR 148", "ISO-8859-9"},        // Turkish
        {"ISO_IR 166", "ISO-8859-11"},       // Thai
        {"ISO 2022 IR 6", default_encoding}, // alias for latin_1 too
        {"ISO 2022 IR 13", "Shift_JIS"},
        {"ISO 2022 IR 87", "ISO-2022-JP"},
        {"ISO 2022 IR 100", "ISO-8859-1"},
        {"ISO 2022 IR 101", "ISO-8859-2"},
        {"ISO 2022 IR 109", "ISO-8859-3"},
        {"ISO 2022 IR 110", "ISO-8859-4"},
        {"ISO 2022 IR 126", "ISO-8859-7"},
        {"ISO 2022 IR 127", "ISO-8859-6"},
        {"ISO 2022 IR 138", "ISO-8859-8"},
        {"ISO 2022 IR 144", "ISO-8859-5"},
        {"ISO 2022 IR 148", "ISO-8859-9"},
        {"ISO 2022 IR 149", "EUC-KR"},
        // {"ISO 2022 IR 159", "iso2022_jp_2"},
        {"ISO 2022 IR 166", "ISO-8859-11"},
        {"ISO 2022 IR 58", "GB2312"},
        {"ISO_IR 192", "UTF8"}, // from Chinese example, 2008 PS3.5 Annex J p1-4
        {"GB18030", "GB18030"},
        {"ISO 2022 GBK", "GBK"},   // from DICOM correction CP1234
        {"ISO 2022 58", "GB2312"}, // from DICOM correction CP1234
        {"GBK", "GBK"}             // from DICOM correction CP1234
    };

    QTextDecoder* decoder = default_decoder();
    if (encodings.size() > 0 && encoding_map.contains(encodings[0])) {
        const char* encoding = encoding_map.value(encodings[0]);
        QTextCodec* codec = QTextCodec::codecForName(encoding);
        if (codec != nullptr)
            decoder = codec->makeDecoder();
    }
    return decoder->toUnicode(value);
}

QString decode_ui(const QByteArray& s) {
    // "If ending on an odd byte boundary, except when used for network
    // negotiation (see PS3.8), one trailing NULL (00H), as a padding character,
    // shall follow the last component in order to align the UID on an even byte
    // boundary." See
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_9.html
    if (s.endsWith('\0'))
        return default_decoder()->toUnicode(s.chopped(1));
    return default_decoder()->toUnicode(s);
}
} // namespace

dcm::DcmFileInfo dcm::get_file_info(QFile& dcm_file) {
    // Open the DICOM file in binary mode
    if (!dcm_file.open(QIODevice::ReadOnly)) {
        throw dcm::ParseException("Failed to open file");
    }
    QDataStream dicom_stream(&dcm_file);

    // 1. Parse the DICOM preamble and check for the magic string

    qint64 s = dicom_stream.skipRawData(DICOM_PREAMBLE_SIZE);
    if (s < 0) {
        throw dcm::ParseException("File "+dcm_file.fileName() + " does not appear to be a DICOM file");
    }
    // Check if the file contains the "DICM" magic string after the preamble
    const char magic[] = DICOM_MAGIC;
    char dicm_check[sizeof(magic)] = {};
    const int magic_len = static_cast<int>(sizeof(magic)) - 1;
    s = dicom_stream.readRawData(dicm_check, magic_len);
    if (s != magic_len || strncmp(dicm_check, magic, magic_len) != 0) {
        throw dcm::ParseException("File "+dcm_file.fileName() + " does not appear to be a DICOM file");
    }

    // 2. Parse Group 0002, which is always in litle-endian, explicit VR mode

    int is_little_endian = 1;
    int explicit_vr = 1;

    dicom_stream.setByteOrder(QDataStream::LittleEndian);
    quint32 group_0002_length = read_group_0002_length(dicom_stream);
    qint64 dicom_set_start = dcm_file.pos() + group_0002_length;
    PRINT_ERR("File header length: %u\n", group_0002_length);

    QByteArray buffer;

    QTextDecoder* default_character_set_decoder = ::default_decoder();

    // Read until the end of the Group 0002 metadata block
    // which is always in the Little Endian format
    while (true) {
        // Read group, element, and length
        DICOMTag tag = read_tag(dicom_stream, 1);
        print_tag(tag);


        if (tag.length != UNDEFINED_LENGTH) {
            buffer = dcm_file.read(tag.length);
        }

        if (tag.vr[0] == 'O' && (tag.vr[1] == 'B' ||  // Case OB
                                 tag.vr[1] == 'W' ||  // Case OW
                                 tag.vr[1] == 'F')) { // Case OF)
            continue;
        }

        PRINT_ERR("\tValue: [%s]\n", buffer.constData());

        if (tag.group == GROUP_0002 && tag.element == TRANSFER_SYNTAX_ELEMENT) {
            QString transfer_syntax = ::decode_ui(buffer);
            if (transfer_syntax == "1.2.840.10008.1.2.2") {
                is_little_endian = 0;
            } else if (transfer_syntax == "1.2.840.10008.1.2") {
                explicit_vr = 0;
            }
            PRINT_ERR("--> Endianness: %s (%s) Explicit=%d\n",
                      buffer.constData(),
                      is_little_endian ? "Little Endian" : "Big Endian",
                      explicit_vr);
            break; // We found what we needed, i.e. the transfer syntax to decode the DICOM Data Set
        }
        if (tag.group != GROUP_0002) {
            break;
        }
    }

    // 3. Read the main DICOM Tag Set to locate the patient id and other tags:

    if (!dcm_file.seek(dicom_set_start)) {
        throw dcm::ParseException("File " + dcm_file.fileName() +
                                  " is invalid DICOM file");
    }

    if (!is_little_endian)
        dicom_stream.setByteOrder(QDataStream::BigEndian);

    dcm::DcmFileInfo info;
    int tags_count = 4;

    // for (const auto& s : QTextCodec::availableCodecs()) {
    //     qDebug() << "CODEC:" << s;
    // }

    QStringList encodings{};

    while (true) {

        DICOMTag tag = read_tag(dicom_stream, explicit_vr);
        print_tag(tag);
        if (tag.length == UNDEFINED_LENGTH) {
            continue;
        }
        // If we reached pixel data (7FE0,0010) abandon the search:
        if (tag.group == 0x7FE0 && tag.element == 0x0010) break;

        buffer = dcm_file.read(tag.length);
        if (tag.group == 0x0008 && tag.element == 0x0005) {
            // Specific Character Set: There can be many
            QString character_set =
                default_character_set_decoder->toUnicode(buffer);
            encodings = character_set.split("\\");
            qDebug().nospace().noquote() << "ENCODINGS: " << encodings;

        } else if (tag.group == 0x0010 && tag.element == 0x0020) {
            // We found the Patient ID
            // LO VR: A character string that may be padded with leading and/or
            // trailing spaces so we trim it:
            info.patient_id = ::decode_string(buffer, encodings).trimmed();
            PRINT_ERR("Patient ID=[%s]\n",
                      info.patient_id.toStdString().c_str());
            if (--tags_count == 0) break;
        } else if (tag.group == 0x0020 && tag.element == 0x000D) {
            // We found the Study Instance UID:
            info.study_uid = ::decode_ui(buffer);
            if (--tags_count == 0) break;
        } else if (tag.group == 0x0020 && tag.element == 0x000E) {
            // We found the Series Instance UID:
            info.series_uid = ::decode_ui(buffer);
            if (--tags_count == 0) break;
        } else if (tag.group == 0x0008 && tag.element == 0x103E) {
            // We found the Series Description, requires decoding
            // LO VR: A character string that may be padded with leading and/or
            // trailing spaces so we trim it:
            info.series_description =
                ::decode_string(buffer, encodings).trimmed();
            if (--tags_count == 0) break;
        }
    }
    return info;
}
