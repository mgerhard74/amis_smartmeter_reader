#pragma once

#include <stddef.h> // size_t
#include <stdint.h>
#include <time.h>

class FileBlobClass
{
    public:
        FileBlobClass(const uint8_t *data, size_t len, const char *filename, const char *md5sum=nullptr, time_t timeStamp=0);
        bool extractIfChanged(bool checkMd5Sum, bool checkTimeStamp);
        bool extractToFile();
        bool remove();
        const char *getFilename();
    private:
        time_t getTimeStamp(); // { return _timeStamp; }
        size_t _len;
        const uint8_t *_data;
        const char *_filename;
        const char *_md5sum;
        time_t _timeStamp;
};


/*
extern FileBlobClass DataBlob_amis_css_gz;
extern FileBlobClass DataBlob_chart_js_gz;
extern FileBlobClass DataBlob_cust_js_gz;
extern FileBlobClass DataBlob_index_html_gz;
extern FileBlobClass DataBlob_jquery371slim_js_gz;
extern FileBlobClass DataBlob_pure_min_css_gz;
*/

bool FileBlobs_extractIfChanged();

// vim:set ts=4 sw=4 et:
