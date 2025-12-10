#include "FileBlob.h"

#include "config.h"
#include "Utils.h"

#include <LittleFS.h>
#include <MD5Builder.h>

extern const time_t __COMPILED_DATE_TIME_UTC_TIME_T__;

extern void writeEvent(String, String, String, String);

FileBlobClass::FileBlobClass(const uint8_t *data, size_t len, const char *filename, const char *md5sum, time_t timeStamp)
{
    _data = data;
    _len = len;
    _filename = filename;
    _md5sum = md5sum;
    _timeStamp = timeStamp;
}

const char *FileBlobClass::getFilename()
{
    return _filename;
}

bool FileBlobClass::remove()
{
    return LittleFS.remove(_filename);
}

# if 1
static time_t blobTimeStamp = (time_t) 0;
static time_t GetTimeStamp()
{
    if (blobTimeStamp != 0) {
        return blobTimeStamp;
    }
    return time(NULL);
}
#else
static time_t _defaultTimeCB(void) { return time(NULL); }
static time_t GetTimeStamp()
{
    // TODO: Return corresponding timestamp
    return __COMPILED_DATE_TIME_UTC_TIME_T__;
}
time_t FileBlobClass::getTimeStamp()
{
    return _timeStamp;
}
#endif

bool FileBlobClass::extractToFile() {
    uint8_t buffer[2048];

    writeEvent("Info", "FileBlobClass::extractToFile()", _filename, String(_len));

    LittleFS.remove(_filename);
    //LittleFS.setTimeCallback(std::bind(&FileBlobClass::getTimeStamp, this)); // TODO: Make this line working
    //LittleFS.setTimeCallback(&GetTimeStamp);
    blobTimeStamp = _timeStamp;
    File f = LittleFS.open(_filename, "w");
    if (!f) {
        return false;
    }
    size_t off = 0, bytesLeft = _len;
    while (bytesLeft != 0) {
        size_t l = std::min(std::size(buffer), bytesLeft);
        memcpy_P(buffer, &_data[off], l);

        if (f.write(buffer, l) != l) {
            f.close();
            writeEvent("Error", "FileBlobClass::extractToFile()", _filename, "write error");
            return false;
        }
        ESP.wdtFeed();
        f.flush();
        yield();
        off += l;
        bytesLeft -= l;
    }
    f.close();
    blobTimeStamp = 0;
    writeEvent("Success", "FileBlobClass::extractToFile()", _filename, String(_len) + " bytes");
    return true;
}


bool FileBlobClass::extractIfChanged(bool checkMd5Sum, bool checkTimeStamp)
{
    LittleFS.setTimeCallback(&GetTimeStamp);

    File f = LittleFS.open(_filename, "r");
    if (!f) {
        return extractToFile();
    }
    size_t size = f.size();
    if (size != _len) {
        f.close();
        return extractToFile();
    }

    if (checkTimeStamp) {
        if (f.getLastWrite() != _timeStamp) {
            f.close();
            //writeEvent("Success", "Timediff", String(f.getLastWrite()), String(_timeStamp));
            return extractToFile();
        }
    }

    if (checkMd5Sum) {
        MD5Builder _md5 = MD5Builder();
        _md5.begin();
        _md5.addStream(f, _len);
        _md5.calculate();

        if (strcmp(_md5.toString().c_str(), _md5sum)) {
            f.close();
            return extractToFile();
        }
    }

    f.close();
    return true;
}


#include "__embed_data_amis_css.h"
static FileBlobClass DataBlob_amis_css_gz(amis_css_gz, amis_css_gz_size, "/amis.css.gz", amis_css_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);
#include "__embed_data_chart_js.h"
static FileBlobClass DataBlob_chart_js_gz(chart_js_gz, chart_js_gz_size, "/chart.js.gz", chart_js_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);
#include "__embed_data_cust_js.h"
static FileBlobClass DataBlob_cust_js_gz(cust_js_gz, cust_js_gz_size, "/cust.js.gz", cust_js_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);
#include "__embed_data_index_html.h"
static FileBlobClass DataBlob_index_html_gz(index_html_gz, index_html_gz_size, "/index.html.gz", index_html_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);
#include "__embed_data_jquery371slim_js.h"
static FileBlobClass DataBlob_jquery371slim_js_gz(jquery371slim_js_gz, jquery371slim_js_gz_size, "/jquery371slim.js.gz", jquery371slim_js_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);
#include "__embed_data_pure_min_css.h"
static FileBlobClass DataBlob_pure_min_css_gz(pure_min_css_gz, pure_min_css_gz_size, "/pure-min.css.gz", pure_min_css_gz_md5, __COMPILED_DATE_TIME_UTC_TIME_T__);


static FileBlobClass* FileBlobs[6] = {
    &DataBlob_amis_css_gz,
    &DataBlob_chart_js_gz,
    &DataBlob_cust_js_gz,
    &DataBlob_index_html_gz,
    &DataBlob_jquery371slim_js_gz,
    &DataBlob_pure_min_css_gz
};


bool FileBlobs_extractIfChanged()
{
    bool r = true;
    //return false;
    for (size_t i=0; i<std::size(FileBlobs); i++) {
        FileBlobClass *blob = FileBlobs[i];
        if (Config.developerModeEnabled) {
            // Do not check timestamp (we've probaly compiled it now) - but check md5
            r &= blob->extractIfChanged(true, false);
        } else {
            // Do not check md5sum - but check timestamp
            r &= blob->extractIfChanged(false, true);
        }
        yield();
    }
    return r;
}


/* vim:set ts=4 et: */
