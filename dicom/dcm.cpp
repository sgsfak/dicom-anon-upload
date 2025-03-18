#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "dcm.h"


#define DICOM_MAGIC "DICM"
#define DICOM_PREAMBLE_SIZE 128
#define GROUP_0002 0x0002
#define TRANSFER_SYNTAX_ELEMENT 0x0010
#define UNDEFINED_LENGTH 0xFFFFFFFF

#ifdef DEBUG
#define PRINT_ERR(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT_ERR(...)
#endif


// DICOM Tag structure
typedef struct
{
    uint16_t group;
    uint16_t element;
    char vr[3];
    uint32_t length;
} DICOMTag;

typedef enum endianness
{
    little_endian,
    big_endian
} endianness_t;

void print_tag(DICOMTag tag)
{
    PRINT_ERR("(%04X,%04X)[%s]{%d}\n", tag.group, tag.element, tag.vr, tag.length);
}

#define UINT16_LE(bytes) (((bytes)[1] << 8) | (bytes)[0])
#define UINT16_BE(bytes) (((bytes)[0] << 8) | (bytes)[1])
#define UINT32_LE(bytes) (((bytes)[3] << 24) | ((bytes)[2] << 16) | ((bytes)[1] << 8) | (bytes)[0])
#define UINT32_BE(bytes) (((bytes)[0] << 24) | ((bytes)[1] << 16) | ((bytes)[2] << 8) | (bytes)[3])

// Function to manually assemble a 16-bit integer from little-endian bytes
uint16_t read_uint16(FILE *dicom_file, int big_endian)
{
    uint8_t bytes[2];
    fread(bytes, 1, 2, dicom_file);
    return big_endian ? UINT16_BE(bytes) : UINT16_LE(bytes);
}
uint32_t read_uint32(FILE *dicom_file, int big_endian)
{
    uint8_t bytes[4];
    fread(bytes, 1, 4, dicom_file);
    return big_endian ? UINT32_BE(bytes) : UINT32_LE(bytes);
}

// Function to parse and read DICOM tags
// actually ony the tag element, VR (optional, based on the explicit param)
// and the length are read
DICOMTag read_tag(FILE *dicom_file, int explicit_vr, int big_endian)
{
    DICOMTag tag;
    memset(&tag, 0, sizeof(DICOMTag));

    tag.group = read_uint16(dicom_file, big_endian) ;
    tag.element = read_uint16(dicom_file, big_endian);

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
        tag.length = read_uint32(dicom_file, big_endian);
        return tag;
    }
    // OK, now we are in explicit mode, so we read the VR
    fread(tag.vr, 1, 2, dicom_file);
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
        read_uint16(dicom_file, big_endian); // Reserved, it should be 0x0000, so IGNORE THIS
        tag.length = read_uint32(dicom_file, big_endian);
    }
    else
        tag.length = read_uint16(dicom_file, big_endian);

    return tag;
}
uint32_t read_group_0002_length(FILE *dicom_file)
{

    DICOMTag tag = read_tag(dicom_file, 1, 0);
    print_tag(tag);
    assert(tag.group == GROUP_0002 && tag.element == 0 && tag.length == 4);

    uint32_t header_length = read_uint32(dicom_file, 0);
    return header_length;
}

#define MY_REALLOC(_buf, _curr_size, _new_size) do { \
        if (_new_size > _curr_size) {                \
            _buf = (char*) realloc(_buf, _new_size);         \
            _curr_size = _new_size;                  \
            PRINT_ERR("  * incr %zu\n", _curr_size); \
        }                                            \
        memset(_buf, 0, _curr_size);                 \
    } while (0)


// Function to check the DICOM preamble and validate the "DICM" magic string
void parse_dicom_preamble(FILE *dicom_file, int* ok)
{
    *ok = 0;
    
    // Buffer to store the preamble (first 128 bytes)
    unsigned char preamble[DICOM_PREAMBLE_SIZE];

    // Read the first 128 bytes
    size_t bytesRead = fread(preamble, 1, DICOM_PREAMBLE_SIZE, dicom_file);
    if (bytesRead != DICOM_PREAMBLE_SIZE) {
        PRINT_ERR("Error: Unable to read the preamble from the file.\n");
        return;
    }

    // Check if the file contains the DICM magic string after the preamble
    char dicm_check[5];
    fread(dicm_check, 1, 4, dicom_file);
    dicm_check[4] = '\0'; // Null-terminate for comparison

    if (strcmp(dicm_check, DICOM_MAGIC) == 0) {
        *ok = 1;
        return;
    }
    PRINT_ERR("Invalid DICOM magic string or corrupted file.\n");

}

char* dcm_get_patient_id(const char* file_name, int *ok)
{
    *ok = 0;

    // Open the DICOM file in binary mode
    FILE *dicom_file = fopen(file_name, "rb");
    if (dicom_file == NULL) {
        PRINT_ERR("Failed to open the DICOM file");
        return NULL;
    }
    // Parse the DICOM preamble and check for the magic string
    parse_dicom_preamble(dicom_file, ok);
    if (!ok) {
        fclose(dicom_file);
        return NULL;
    }

    int is_little_endian = 1;
    int explicit_vr = 1;

    uint32_t group_0002_length = read_group_0002_length(dicom_file);
    long dicom_set_start = ftell(dicom_file) + group_0002_length;
    PRINT_ERR("File header length: %d\n", group_0002_length);
    char *value = NULL;
    size_t val_size = 0;

    // Read until the end of the Group 0002 metadata block
    // which is always in the Little Endian format
    while (1)
    {
        // Read group, element, and length
        DICOMTag tag = read_tag(dicom_file, 1, 0);
        print_tag(tag);


        if (tag.length != UNDEFINED_LENGTH) {
            // value = realloc(value, tag.length+1);
            // memset(value, 0, tag.length+1);
            MY_REALLOC(value, val_size, tag.length+1);
            fread(value, 1, tag.length, dicom_file);
        }

        if (strcmp(tag.vr, "OB") == 0 || strcmp(tag.vr, "OW") == 0 || strcmp(tag.vr, "OF") == 0) {
            continue;
        }
        PRINT_ERR("\tValue: [%s]\n", value);

        if (tag.group == GROUP_0002 && tag.element == TRANSFER_SYNTAX_ELEMENT) {
            if (value[tag.length-1] == ' ') value[tag.length-1] = '\0';
            if (strcmp((char*) value, "1.2.840.10008.1.2.2") == 0) {
                is_little_endian = 0;
            }
            else if (strcmp((char*) value, "1.2.840.10008.1.2") == 0) {
                explicit_vr = 0;
            }
            PRINT_ERR("--> Endianness: %s (%s) Explicit=%d\n",
                      value, 
                      is_little_endian ? "Little Endian" : "Big Endian",
                      explicit_vr);
        }
        if (tag.group != GROUP_0002) {
            break;
        }
    }

    fseek(dicom_file, dicom_set_start, SEEK_SET);

    while (!feof(dicom_file)) {

        DICOMTag tag = read_tag(dicom_file, explicit_vr, is_little_endian ? 0 : 1);
        print_tag(tag);
        if (tag.length == UNDEFINED_LENGTH) {
            continue;
        }
        // If we reached pixel data (7FE0,0010) abandon the search:
        if (tag.group == 0x7FE0 && tag.element == 0x0010) break;

        MY_REALLOC(value, val_size, tag.length+1);
        fread(value, 1, tag.length, dicom_file);
        PRINT_ERR("\tValue: [%s]\n", value);
        if (tag.group == 0x0010 && tag.element == 0x0020) {
            // We found the Patient ID! Remove the last space if it's there
            // to make sure that the length is even, and return it:
            if (value[tag.length-1] == ' ') value[tag.length-1] = '\0';
            fclose(dicom_file);
            return value;
        }
    }
    free(value);
    fclose(dicom_file);
    return NULL;
}
