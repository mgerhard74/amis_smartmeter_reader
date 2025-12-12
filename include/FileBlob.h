#pragma once

#include <stddef.h> // size_t
#include <stdint.h>
#include <time.h>

#include <LittleFS.h>

// FILEBLOB_WRITE_MD_INO_FILE:
//      0 ... md5sum is computed (needs long time)
//      1 ... md5sum is written into extra file
#define FILEBLOB_WRITE_MD_INO_FILE  1

class FileBlobClass
{
    public:
        FileBlobClass(const uint8_t *data, size_t len, const char *filename, const char *md5sum=nullptr, time_t timeStamp=0);
        //bool extractIfChanged(bool checkMd5Sum, bool checkTimeStamp);
        //bool extractToFile();
        void checkIfChanged(bool checkMd5Sum, bool checkTimeStamp);
        bool remove();
        bool isChanged();
        const char *getFilename();
        bool extractToFileNextBlock();
    private:
        time_t getTimeStamp(); // { return _timeStamp; }
        size_t _len;
        size_t _bytesWritten;
        bool   _isChanged;
        const uint8_t *_data;
        const char *_filename;
        const char *_md5sum;
        time_t _timeStamp;
#if (FILEBLOB_WRITE_MD_INO_FILE)
        char _filenameMd5[LFS_NAME_MAX]; // this includes already trailing '\0' --> so only 31 chars for filename
#endif
};


class FileBlobsClass
{
    public:
        void init();
        void checkIsChanged();
        void loop();            // this proberly extracts
        void remove(bool force);

    private:
        FileBlobClass* _fileBlobs[6];
        size_t _currentBlob;
        bool _extractionInProgress;
};
extern FileBlobsClass FileBlobs;


/*
extern FileBlobClass DataBlob_amis_css_gz;
extern FileBlobClass DataBlob_chart_js_gz;
extern FileBlobClass DataBlob_cust_js_gz;
extern FileBlobClass DataBlob_index_html_gz;
extern FileBlobClass DataBlob_jquery371slim_js_gz;
extern FileBlobClass DataBlob_pure_min_css_gz;
*/


// vim:set ts=4 sw=4 et:
