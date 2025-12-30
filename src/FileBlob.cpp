#include "FileBlob.h"

#include "config.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_BIT_SYSTEM
#include "Utils.h"

#if not (FILEBLOB_WRITE_MD_INTO_FILE)
#include <MD5Builder.h>
#endif

extern const time_t __COMPILED_DATE_TIME_UTC_TIME_T__;

FileBlobClass::FileBlobClass(const uint8_t *data, size_t len, const char *filename, const char *md5sum, time_t timeStamp)
{
    _data = data;
    _len = len;
    _filename = filename;
    _md5sum = md5sum;
    _timeStamp = timeStamp;
#if (FILEBLOB_WRITE_MD_INTO_FILE)
    // Create a filename: _filename + ".md5" within 31 bytes (possible hrink _filename)
    strncpy(_filenameMd5, _filename, sizeof(_filenameMd5)-2);
    if (strlen(_filename) > sizeof(_filenameMd5)-5) {
        _filenameMd5[sizeof(_filenameMd5)-5] = 0;
    }
    strcat(_filenameMd5, ".md5"); // NOLINT
#endif
}

const char *FileBlobClass::getFilename()
{
    return _filename;
}

bool FileBlobClass::remove()
{
    bool r = true;
#if (FILEBLOB_WRITE_MD_INTO_FILE)
    r &= LittleFS.remove(_filenameMd5);
#endif
    r &= LittleFS.remove(_filename);
    return r;
}

void checkIfChanged();


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
    // TODO(anyone): Return corresponding timestamp
    return __COMPILED_DATE_TIME_UTC_TIME_T__;
}
time_t FileBlobClass::getTimeStamp()
{
    return _timeStamp;
}
#endif


// Extrahieren mehrere Dateien brauch sehr lange
// Den HW Watchdog disablen/enablen
//    void hw_wdt_disable(){ *((volatile uint32_t*) 0x60000900) &= ~(1); /* Hardware WDT OFF */ }
//    void hw_wdt_enable() { *((volatile uint32_t*) 0x60000900) |= 1;    /* Hardware WDT ON  */ }
// ist sehr unsauber!
// Also: Blockweises (1kB) Schreiben in der loop()
bool FileBlobClass::extractToFileNextBlock()
{
    uint8_t buffer[1024];

    if (_bytesWritten == 0) {
        remove();
    }

    blobTimeStamp = _timeStamp;

    File f = LittleFS.open(_filename, (_bytesWritten == 0) ?"w" :"a");
    if (!f) {
        LOG_EP("FileBlobClass::extractToFile() %s open error!", _filename);
        _bytesWritten = 0; // stop updating the file even on failure
        _isChanged = false;
        blobTimeStamp = 0;
        return false;
    }
    size_t off = _bytesWritten,  bytesLeft = _len - _bytesWritten;
    while (bytesLeft != 0) {
        size_t l = std::min(std::size(buffer), bytesLeft);
        memcpy_P(buffer, &_data[off], l);

        if (f.write(buffer, l) != l) {
            f.close();
            LOG_EP("FileBlobClass::extractToFile() %s write error!", _filename);
            _bytesWritten = 0; // stop updating the file even on failure
            _isChanged = false;
            blobTimeStamp = 0;
            return false;
        }
        ESP.wdtFeed();
        f.flush();
        yield();
        off += l;
        bytesLeft -= l;
        _bytesWritten += l;
        break; // Just write one block as otherwise hardware watchdog get raised
    }
    f.close();

#if (FILEBLOB_WRITE_MD_INTO_FILE)
    if (_bytesWritten == _len) {
        // whole file has been written
        f = LittleFS.open(_filenameMd5, "w");
        if (f) { // ignore any errors on writing the md5sum
            f.write((const uint8_t *)_md5sum, 32);
            f.close();
        }
    }
#endif
    blobTimeStamp = 0;

    if (_bytesWritten == _len) {
        _bytesWritten = 0;
        _isChanged = false; // whole file has been written --> reset _isChanged flag
        LOG_IP("FileBlobClass::extractToFile() %s: %u bytes written.", _filename, _len);
    }
    return true;
}


void FileBlobClass::checkIfChanged(bool checkMd5Sum, bool checkTimeStamp)
{
    _isChanged = false;

    File f = LittleFS.open(_filename, "r");
    if (!f) {
        _isChanged = true;
        return;
    }
    size_t size = f.size();
    if (size != _len) {
        f.close();
        _isChanged = true;
        LOG_VP("FileBlobClass::checkIfChanged()-site %u vs %u", size, _len);
        return;
    }

    if (checkTimeStamp) {
        time_t lw = f.getLastWrite();
        if (lw != _timeStamp) {
            f.close();
            _isChanged = true;
            LOG_VP("FileBlobClass::checkIfChanged()-timestamp %llu vs %llu", lw, _timeStamp);
            return;
        }
    }

#if (FILEBLOB_WRITE_MD_INTO_FILE)
    // As real md5sum computation needs so long, we write the md5sum into an extra file
    f.close();
    if (checkMd5Sum) {
        uint8_t md5_strdata[33];
        memset(md5_strdata, 0, sizeof(md5_strdata)); // we can use it later as c_str()
        File f = LittleFS.open(_filenameMd5, "r");
        if (f && f.size() == 32) {
            f.read(md5_strdata, 32);
            f.close();
        }
        if (memcmp(md5_strdata, _md5sum, 32)) {
            LOG_VP("FileBlobClass::checkIfChanged()-md5sum %s vs %s", _md5sum, (char*)md5_strdata);
            _isChanged = true;
        }
    }
#else
    if (checkMd5Sum) {
        MD5Builder _md5 = MD5Builder();
        _md5.begin();
        _md5.addStream(f, _len);
        _md5.calculate();

        if (strcmp(_md5.toString().c_str(), _md5sum)) {
            f.close();
            _isChanged = true;
            return;
        }
    }
    f.close();
#endif
}

bool FileBlobClass::isChanged()
{
    return _isChanged;
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


void FileBlobsClass::init()
{
    _fileBlobs[0] = &DataBlob_amis_css_gz;
    _fileBlobs[1] = &DataBlob_cust_js_gz;
    _fileBlobs[2] = &DataBlob_index_html_gz;
    _fileBlobs[3] = &DataBlob_jquery371slim_js_gz;
    _fileBlobs[4] = &DataBlob_pure_min_css_gz;
    _fileBlobs[5] = &DataBlob_chart_js_gz;

    LittleFS.setTimeCallback(&GetTimeStamp);

    _extractionInProgress = false;
}


void FileBlobsClass::checkIsChanged()
{
    if (_extractionInProgress) {
        return;
    }
    for (size_t i=0; i<std::size(_fileBlobs); i++) {
        FileBlobClass *blob = _fileBlobs[i];
        /*
        if (Config.developerModeEnabled) {
            // Do not check timestamp (we've probaly compiled it now) - but check md5
            blob->checkIfChanged(true, false);
        } else {
            blob->checkIfChanged(true, true);
        }
        */
        blob->checkIfChanged(true, false);
    }
    _currentBlob = 0;
    _extractionInProgress = true; // start extraction
}


void FileBlobsClass::loop()
{
    if(!_extractionInProgress) {
        return;
    }

    FileBlobClass *blob = _fileBlobs[_currentBlob];
    if (blob->isChanged()) {
        blob->extractToFileNextBlock(); // this can update isChanged() flag
    }
    if (!blob->isChanged()) {
        _currentBlob++;
        if (_currentBlob == std::size(_fileBlobs)) {
            _extractionInProgress = false; // last blob is on disc -> extraction finished
        }
    }
}

void FileBlobsClass::remove(bool force)
{
    if (force || !_extractionInProgress) {
        for (size_t i=0; i < std::size(_fileBlobs); i++) {
            FileBlobClass *blob = _fileBlobs[i];
            blob->remove();
        }
    }
    _extractionInProgress = false;
}

#if 0
void FileBlobsClass::removeNotPackedFiles()
{
    // If a filename in the blobs ends with ".gz" and the
    // file without .gz is available in the filesystem then remove the file without ".gz"
    FileBlobClass *blob;
    char _filenameOld[LFS_NAME_MAX];

    for (size_t i=0; i < std::size(_fileBlobs); i++) {
        blob = _fileBlobs[i];
        strcpy(_filenameOld, blob->getFilename()); // NOLINT
        if (strlen(_filenameOld) > 3 && strcmp(_filenameOld+strlen(_filenameOld)-3, ".gz") == 0) {
            _filenameOld[strlen(_filenameOld) - 3] = 0;
            if (Utils::fileExists(_filenameOld)) {
                LittleFS.remove(_filenameOld);
            }
        }
    }
}
#endif

FileBlobsClass FileBlobs;

/* vim:set ts=4 et: */
