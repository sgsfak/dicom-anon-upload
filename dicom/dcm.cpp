#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dcm.h"
#include <QDataStream>
#include <QFile>
#include <QDebug>

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


// DICOM Tag structure
struct DICOMTag
{
    quint16 group;
    quint16 element;
    char vr[3];
    quint32 length;

    DICOMTag(): group(0), element(0), length(UNDEFINED_LENGTH) {
        memset(this->vr, 0, 3);
    }

    QString toString() {
        return QString( "(%1,%2)").arg(QString::number(this->group, 16), 4, '0').arg(QString::number(this->element, 16), 4, '0');
    }
};


void print_tag(DICOMTag tag)
{
#ifdef DEBUG
    qDebug().noquote() << tag.toString() << "[" << tag.vr << "]" << "{" << tag.length << "}";
#endif
}

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
    if (!explicit_vr
        || (tag.group == 0xFFFE && tag.element == 0xE000)
        || (tag.group == 0xFFFE && tag.element == 0xE00D)
        || (tag.group == 0xFFFE && tag.element == 0xE0DD)
    ) {
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
    if (strcmp(tag.vr, "OB") == 0 ||
        strcmp(tag.vr, "OW") == 0 ||
        strcmp(tag.vr, "OF") == 0 ||
        strcmp(tag.vr, "SQ") == 0 ||
        strcmp(tag.vr, "UT") == 0 ||
        strcmp(tag.vr, "UN") == 0)
    {
        read_uint16(dicom_stream); // Reserved, it should be 0x0000, so IGNORE THIS
        tag.length = read_uint32(dicom_stream);
    }
    else
        tag.length = read_uint16(dicom_stream);

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

QByteArray dcm::get_patient_id(QFile& dcm_file)
{

    // fprintf(stderr, "Parsing file %s\n", file_name);
    // Open the DICOM file in binary mode
    if (!dcm_file.open(QIODeviceBase::ReadOnly)) {
        throw dcm::ParseException("Failed to open file");
    }
    QDataStream dicom_stream(&dcm_file);

    // 1. Parse the DICOM preamble and check for the magic string

    qint64 s = dicom_stream.skipRawData(DICOM_PREAMBLE_SIZE);
    if (s < 0) {
        throw dcm::ParseException("File "+dcm_file.fileName() + " does not appear to be a DICOM file");
    }
    // Check if the file contains the DICM magic string after the preamble
    char dicm_check[5] = {0};
    s = dicom_stream.readRawData(dicm_check, 4);
    if (s != 4 || strcmp(dicm_check, "DICM") != 0) {
        throw dcm::ParseException("File "+dcm_file.fileName() + " does not appear to be a DICOM file");
    }
    else {
        // qDebug().noquote() << QString("Out file %1 appears to be DICOM").arg(dcm_file.fileName());
    }

    // 2. Parse Group 0002, which is always in litle-endian, explicit VR mode

    int is_little_endian = 1;
    int explicit_vr = 1;

    dicom_stream.setByteOrder(QDataStream::LittleEndian);
    quint32 group_0002_length = read_group_0002_length(dicom_stream);
    qint64 dicom_set_start = dcm_file.pos() + group_0002_length;
    PRINT_ERR("File header length: %u\n", group_0002_length);

    QByteArray buffer;

    // Read until the end of the Group 0002 metadata block
    // which is always in the Little Endian format
    while (1)
    {
        // Read group, element, and length
        DICOMTag tag = read_tag(dicom_stream, 1);
        print_tag(tag);


        if (tag.length != UNDEFINED_LENGTH) {
            buffer = dcm_file.read(tag.length);
        }

        if (strcmp(tag.vr, "OB") == 0 || strcmp(tag.vr, "OW") == 0 || strcmp(tag.vr, "OF") == 0) {
            continue;
        }

        PRINT_ERR("\tValue: [%s]\n", buffer.constData());

        if (tag.group == GROUP_0002 && tag.element == TRANSFER_SYNTAX_ELEMENT) {
            if (buffer.endsWith(' '))
                buffer[tag.length-1] = '\0';
            buffer.append('\0'); // Make it null terminated
            if (strcmp(buffer.constData(), "1.2.840.10008.1.2.2") == 0) {
                is_little_endian = 0;
            }
            else if (strcmp(buffer.constData(), "1.2.840.10008.1.2") == 0) {
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

    // 3. Read the main DICOM Tag Set to locate the patient id:

    dcm_file.seek(dicom_set_start);

    if (!is_little_endian)
        dicom_stream.setByteOrder(QDataStream::BigEndian);

    while (1) {

        DICOMTag tag = read_tag(dicom_stream, explicit_vr);
        print_tag(tag);
        if (tag.length == UNDEFINED_LENGTH) {
            continue;
        }
        // If we reached pixel data (7FE0,0010) abandon the search:
        if (tag.group == 0x7FE0 && tag.element == 0x0010) break;

        buffer = dcm_file.read(tag.length);
        buffer.append('\0');
        PRINT_ERR("\tValue: [%s]\n", buffer.constData());
        if (tag.group == 0x0010 && tag.element == 0x0020) {
            // We found the Patient ID! Remove the last space if it's there
            // to make sure that the length is even, and return it:
            if (buffer.endsWith(' '))
                buffer[tag.length-1] = '\0';
            return buffer;
        }
    }
    return "";
}
