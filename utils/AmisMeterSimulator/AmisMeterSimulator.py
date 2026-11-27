#! /usr/bin/env python3
## vim:set ts=4 sw=4 et: -*- coding: utf-8 -*-
#
# Simulator eines Siemens TD-3511/TD-3512 (AMIS, Netz OÖ) an der seriellen Schnittstelle
# zum Entwickeln und Testen des AMIS-Reader
#
# Kann auch die Daten eines anderen Zählers abfragen (Proxy-Mode) und auf der
# Seriellen ausgeben - Abfrage erfolgt via REST Schnittstelle
#
# Optionen für:
# Serielle, Key, Serialnummer, Zeit-Offset, Remote-Rest-API
#
# Hardware: Beim AMIS-Leser sind leider die RX und TX Leitungen nicht direkt an der
#           USB Schnittstelle verfügbar.
#           Man muss sich den Flasher (https://www.mitterbaur.at/amis/AmisReaderflashen.pdf) zusammenbasteln,
#           damit man dann mit dem AMIS-Leser via serieller Schnittstelle kommunizieren kann.
#
# Defaultkey = "1234567890abcdef1234567890abcdef"


import os, sys, time
import getopt


# https://pythonhosted.org/pyserial/pyserial_api.html
# https://pyserial.readthedocs.io/en/latest/pyserial_api.html
import serial # yum install python3-pyserial.noarch

# https://onboardbase.com/blog/aes-encryption-decryption/
# https://pycryptodome.readthedocs.io/en/latest/src/cipher/cipher.html
# https://pycryptodome.readthedocs.io/en/latest/src/cipher/aes.html
#from Crypto.Cipher import AES
from Cryptodome.Cipher import AES # yum install python3-pycryptodomex.x86_64


import base64, urllib.request, json


# Tests zeigten:
# Der Zähler schickt jede Minute einfach einmalig ein SND_NKE
# 0m 25s.193    INFO    reader  Received SND_NKE, sending ACK
# 1m 25s.206    INFO    reader  Received SND_NKE, sending ACK
# 2m 25s.168    INFO    reader  Received SND_NKE, sending ACK
# 3m 25s.178    INFO    reader  Received SND_NKE, sending ACK
# 4m 25s.211    INFO    reader  Received SND_NKE, sending ACK
# 5m 25s.191    INFO    reader  Received SND_NKE, sending ACK

class opts:
    verbose = 0
    port = "/dev/ttyUSB0"
    serialNumber = "-PyAmisMeterSimulator-" # set to "" to disable serial number exchange
    key = "1234567890abcdef1234567890abcdef"
    remoteUrl = None # a remote counter url - eg: "http://192.168.X.Y/rest"
    remoteUsername = ""
    remotePassword = ""
    timeoffset = 0 # Data submitted with current time or time from remote. Use this to adjust the time seconds


#class globs:
#    __UNUSED__ = None


def log(*args, **kwargs):
    prefix = time.asctime() + " "
    print(prefix + " ".join(map(str,args)), **kwargs)

class AmisSerial:
    def __init__(self, port):
        self.serial = None
        self.port = port

    def open(self, mode):
        if mode == 0:
            # serialnumber transfer
            bd = 300
            bs = serial.SEVENBITS       # 300 7E1
            pa = serial.PARITY_EVEN
            sb = serial.STOPBITS_ONE
        elif mode == 1:
            # counter numbers transfer
            bd = 9600
            bs = serial.EIGHTBITS       # 9600 8E1
            pa = serial.PARITY_EVEN
            sb = serial.STOPBITS_ONE
        else:
            raise Exception("Invalid mode")

        if True:
            if self.serial is None:
                self.serial = serial.Serial()
            else:
                self.serial.close()
            self.serial.port = self.port
            self.serial.baudrate = bd
            self.serial.bytesize = bs
            self.serial.parity = pa
            self.serial.stopbits = sb
            self.serial.open()
            return

        if self.serial.baudrate != bd:
            self.serial.baudrate = bd
        if self.serial.bytesize != bs:
            self.serial.bytesize = bs
        if self.serial.parity != pa:
            self.serial.parity = pa
        if self.serial.stopbits != sb:
            self.serial.stopbits = sb

        if self.serial is None:
            # first acces - we
            self.serial = serial.Serial()
            self.serial.port = self.port
            self.serial.baudrate = bd
            self.serial.bytesize = bs
            self.serial.parity = pa
            self.serial.stopbits = sb
            self.serial.open()
            return

        if self.serial.baudrate != bd:
            self.serial.baudrate = bd
        if self.serial.bytesize != bs:
            self.serial.bytesize = bs
        if self.serial.parity != pa:
            self.serial.parity = pa
        if self.serial.stopbits != sb:
            self.serial.stopbits = sb

    def close(self):
        if self.serial:
            self.serial.close()
            self.serial = None

    def printSerialIncomingData(self, v):
        # try printing data as utf-8 string
        # if not possible: print the bytes
        if type(v) == type("A") or type(v) == type(u"A"):
            log("IN<-[%d] '%s'" % (len(v), v))
            return
        try:
            s = v.decode("utf-8")
            log("IN<-[%d] '%s'" % (len(v), s))
        except:
            s = v.decode("utf-8", errors='ignore')
            if s and len(s):
                log("IN<-[%d] '%s'" % (len(v), s))
            else:
                # display as bytes
                log("IN<-[%d] %s" % (len(v), str(v)))


    def readUntil(self, eol, timeout=None):
        self.serial.timeout = timeout
        ts = time.time()
        r = self.serial.read_until(eol)
        if not r:
            return r

        if r.endswith(eol):
            if r == eol:
                if opts.verbose >= 2:
                    self.printSerialIncomingData(r)
                return r

        # Hier haben wir was bekommen, aber nicht damit gerechnet
        self.serial.timeout = 0.15
        rr = r
        while r:
            r = self.serial.read(1)
            rr += r
        self.printSerialIncomingData(rr)
        if r.endswith(eol):
            return eol
        return b""

    def writeAndFlush(self, data, printstr="", newline=False):
        if type(data) == type(u"a"):
            data = bytes(data, "utf-8")

        if not printstr:
            try:
                printstr = data.decode("utf-8")
                printstr = "'" + printstr + "'"
                printstr = printstr.replace("\r", "\\r")
                printstr = printstr.replace("\n", "\\n")
            except:
                printstr = str(data)
        else:
            pass

        if newline:
            printstr += "\n"

        if opts.verbose >= 1:
            log("OUT->[%d] %s" % (len(data), printstr), end="")
        r = self.serial.write(data)
        self.serial.flush()
        #while self.serial.out_waiting > 0:
        #    time.sleep(0.01) # 1ms is about ~1Byte @ 9600 8E1
        return r

    def trySendSerialnumber(self, serialNumber, timeoutSec):
        self.open(0)
        log("Waiting for request of serialnumber @%dBd" % self.serial.baudrate)
        start_ts = int(time.time())
        data = b""
        while int(time.time()) < start_ts + timeoutSec:
            data += self.readUntil( b"/?!\r\n", 1)
            if data.endswith(b"/?!\r\n"):
                self.writeAndFlush("/SAT6%s\r\n" % serialNumber, "", True)
                log("Serialnumber '%s' sent." % serialNumber)
                time.sleep(0.5)
                return True
        log("ERROR: Could not send serialnumber (no request received)")
        return False

    def trySendCounterData(self, amis, updateCounterCallback):
        self.open(1)
        log("Waiting request to start sending counter datas @%dBd" % self.serial.baudrate)

        prevSendingState = None
        state = 0
        errcnt = 0
        lastUdSentTime = -1

        data = b""

        needNewLineNextTime = False

        while True:
            if state == 0: # send SND_NKE (Initalize the slave(=our reader))
                prevSendingState = 0
                if needNewLineNextTime:
                    print()
                    needNewLineNextTime = False
                self.writeAndFlush(b"\x10\x40\xF0\x30\x16", "Sending SND_NKE and wait for ACK", True) #expect 0xe5 as answer
                state = 1
                continue

            if state == 1:
                data += self.readUntil(b"\xe5", 1.5)
                if data.endswith(b"\xe5"):
                    if prevSendingState == 0:
                        if needNewLineNextTime:
                            print()
                            needNewLineNextTime = False
                        log("Got ACK (0xE5)! Start sending counter datas...")
                    state = 2
                    errcnt = 0
                    data = b""
                else:
                    if needNewLineNextTime:
                        print()
                        needNewLineNextTime = False
                    log("ERROR: Missed ACK (0xE5)")
                    errcnt += 1

                    # after 10.7 (7*1.5) seconds not got any 0xe5: stop sending SND_UDs or SND_NKEs and exit
                    if errcnt > 7:
                        break
                    state = prevSendingState
                    continue

            if state == 2: # build and send SND_UD (Send UserData to slave)
                prevSendingState = 2
                # Wait start of next second and honor the millis
                t = time.time()
                if int(t) == lastUdSentTime:
                    time.sleep(int(t)+1-t)
                lastUdSentTime = int(time.time())

                if updateCounterCallback:
                    haveValidData = updateCounterCallback(amis)
                else:
                    haveValidData = True

                if haveValidData:
                    data = amis.getUDTelegram()
                    self.writeAndFlush(data, "sending UD (meterdata) 1.7.0=%d, 2.7.0=%d....\r" % (amis.c1_7_0, amis.c2_7_0))
                    needNewLineNextTime = True

                if not updateCounterCallback:
                    amis.incCounters() # just increment them
                state = 1
                continue
        if needNewLineNextTime:
            print()
            needNewLineNextTime = False


class AmisMeter:
    difVifs = [
            b"\x04\x03",                    # M-Bus DIF/VIFs - 1.8.0 für Zählerstand Energie A+ [Wh]
            b"\x04\x83\x3c",                # M-Bus DIF/VIFs - 2.8.0 für Zählerstand Energie A- [Wh]
            b"\x84\x10\xFB\x82\x73",        # M-Bus DIF/VIFs - 3.8.1 für Zählerstand Energie R+ [Wh]
            b"\x84\x10\xFB\x82\xf3\x3c",    # M-Bus DIF/VIFs - 4.8.1 für Zählerstand Energie R+ [Wh]
            b"\x04\x2b",                    # M-Bus DIF/VIFs - 1.7.0 für momentane Wirkleistung P+
            b"\x04\xab\x3c",                # M-Bus DIF/VIFs - 2.7.0 für momentane Wirkleistung P-
            b"\x04\xfb\x14",                # M-Bus DIF/VIFs - 3.7.0 für momentane Blindleistung Q+
            b"\x04\xfb\x94\x3c",            # M-Bus DIF/VIFs - 4.7.0 für momentane Blindleistung Q-
            b"\x04\x83\xff\x04",            # M-Bus DIF/VIFs - 1.128.0 für Inkassozählwerk
            b"\x06\x6d",                    # M-Bus DIF/VIFs - CP48 Date/Time
        ]
    counterNames = [
            "1.8.0",
            "2.8.0",
            "3.8.1",
            "4.8.1",
            "1.7.0",
            "2.7.0",
            "3.7.0",
            "4.7.0",
            "1.128.0",
            "CP48 Date/Time",
        ]

    def __init__(self, key, serialNumber, timeStampOffset=0):
        self.serialNumber = serialNumber
        self.key = key
        self.accessNumber = 0
        self.counters = [0] * 9
        self._timeStamp = None
        self._timeStampOffset = timeStampOffset

    def setCounter(self, index, value):
        if index == 8:
            # signed
            assert(value <= 0x7fffffff and value >= -0x80000000)
        else:
            # unsigned
            assert(value >= 0 and value <= 0xffffffff)
        self.counters[index] = value

    @property
    def c1_8_0(self):
        return self.counters[0]
    @c1_8_0.setter
    def c1_8_0(self, value):
        self.setCounter(0, value)
    @property
    def c2_8_0(self):
        return self.counters[1]
    @c2_8_0.setter
    def c2_8_0(self, value):
        self.setCounter(1, value)
    @property
    def c3_8_1(self):
        return self.counters[2]
    @c3_8_1.setter
    def c3_8_1(self, value):
        self.setCounter(2, value)
    @property
    def c4_8_1(self):
        return self.counters[3]
    @c4_8_1.setter
    def c4_8_1(self, value):
        self.setCounter(3, value)

    @property
    def c1_7_0(self):
        return self.counters[4]
    @c1_7_0.setter
    def c1_7_0(self, value):
        self.setCounter(4, value)
    @property
    def c2_7_0(self):
        return self.counters[5]
    @c2_7_0.setter
    def c2_7_0(self, value):
        self.setCounter(5, value)
    @property
    def c3_7_0(self):
        return self.counters[6]
    @c3_7_0.setter
    def c3_7_0(self, value):
        self.setCounter(6, value)
    @property
    def c4_7_0(self):
        return self.counters[7]
    @c4_7_0.setter
    def c4_7_0(self, value):
        self.setCounter(7, value)

    @property
    def c1_128_0(self):
        return self.counters[8]
    @c1_128_0.setter
    def c1_128_0(self, value):
        self.setCounter(8, value)


    @property
    def timeStamp(self):
        return self._timeStamp
    @timeStamp.setter
    def timeStamp(self, value):
        self._timeStamp = int(value)


    def incCounters(self):
        for i in range(len(self.counters)):
            self.setCounter(i, self.counters[i] + 1)

    def getCP48DT_IFormat(self, timeStamp=None):
        r = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00]

        ts = timeStamp               # use timestamp from function call
        if ts is None:
            ts = self._timeStamp     # use timestamp from the object
            if ts is None:
                ts = int(time.time())# use current time

        t = time.localtime(ts + opts.timeoffset)
        yy = t.tm_year % 100
        mm = t.tm_mon;
        dd = t.tm_mday;
        h = t.tm_hour;
        m = t.tm_min;
        s = t.tm_sec
        if s >= 60:
            s = 59
        wday = t.tm_wday + 1 # MBUS: 1=MON,7=SUN // PY: 0=MON,6=SUN

        r[0] = s
        if t.tm_isdst:
            r[0] |= 0x40;
        r[1] = m;
        r[2] = h;
        r[2] |= wday << 5;
        r[3] = dd;
        r[3] |= (yy & 0x07) << 5;
        r[4] = mm;
        r[4] |= (yy & 0x78) << 1;

        #TODO: r[5] (Offset DST füllen)

        return bytes(r)

    def getXint32Data(self, number, difVif, isSigned):
        if isSigned:
            assert(number <= 0x7fffffff and number >= -0x80000000)
        else:
            assert(number <= 0xffffffff and number >= 0)

        r = bytes(difVif) + int.to_bytes(number, 4, 'little', signed=isSigned)
        return r

    def decryptData(self, data): # this method is only for testing !
        assert(len(data) >= 19 + 80)
        IV_OFF = 7
        iv = [ data[IV_OFF+4], data[IV_OFF+5], data[IV_OFF+0], data[IV_OFF+1],
               data[IV_OFF+2], data[IV_OFF+3], data[IV_OFF+6], data[IV_OFF+7],
               data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8],
               data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8] ]
        #print("KEY:", bytes.fromhex(self.key))
        #print("IV: ", bytes(iv))
        aes = AES.new(bytes.fromhex(self.key), AES.MODE_CBC, iv=bytes(iv))
        #print ("IV:", aes.iv)

        DATA_OFF = 19; DATA_LEN = 80
        decoded = aes.decrypt(data[DATA_OFF:DATA_OFF+DATA_LEN])
        #print(hex(decoded[0]), hex(decoded[1]))
        #print(decoded)
        return bytes(data[:DATA_OFF] + decoded + data[DATA_OFF+DATA_LEN:])

    def encryptData(self, data):
        assert(len(data) >= 19 + 80)

        IV_OFF = 7
        iv = [ data[IV_OFF+4], data[IV_OFF+5], data[IV_OFF+0], data[IV_OFF+1],
               data[IV_OFF+2], data[IV_OFF+3], data[IV_OFF+6], data[IV_OFF+7],
               data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8],
               data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8], data[IV_OFF+8] ]

        aes = AES.new(bytes.fromhex(self.key), AES.MODE_CBC, iv=bytes(iv))
        DATA_OFF = 19; DATA_LEN = 80
        encrypted = aes.encrypt(data[DATA_OFF:DATA_OFF+DATA_LEN])
        return bytes(data[:DATA_OFF] + encrypted + data[DATA_OFF+DATA_LEN:])

    def getMBUS_SND_UD(self):
        # build the userdata (UD) telegram
        data = bytes([0x2f, 0x2f])
        data += bytes(self.difVifs[9]) + bytes(self.getCP48DT_IFormat())
        self.c1_128_0 = 0 # sonst geht das mit AMIS-Reader V1.28 nicht !
        for i in range(len(self.counters)):
            isSigned = i == len(self.counters)-1 #c1_128_0 ist ein signed int!
            data += self.getXint32Data(self.counters[i], self.difVifs[i], isSigned)
        data += bytes([0x2f, 0x2f])
        assert len(data) == 80
        return data

    def getUDTelegram(self):
        LENGTH = 0x5f # without 4 bytes header(start+length * 2) and 2 bytes tail(crc+stop)
        data = bytes([0x68, LENGTH, LENGTH, 0x68])
        data += bytes([0x53]) # cField
        data += bytes([0xf0]) # primary Adddress
        data += bytes([0x5b]) # variable data response
        data += bytes([0x00,0x00,0x00,0x00]) # Zählernummer?? 0x00, 0x00, 0x00, 0x00
        data += bytes([0x2d,0x4c]) #Hersteller Identifikation EMH, LSB first 0x2d 0x4c
        data += bytes([0x01]) # Version
        data += bytes([0x0e]) # 02 = Medium Elektrizität --> hier 0x0e
        data += bytes([self.accessNumber & 0xff]);  self.accessNumber += 1 # accesscounter (laufender Zähler - erhöht sich bei jeder Ausgabe)
        data += bytes([0x50]) # status;                  // undefined              0x50
        data += bytes([0x05, 0x00]) # signature;              // undefined              0x05
        data += self.getMBUS_SND_UD()
        data = self.encryptData(data)

        # compute checksum
        crc  = 0
        for i in range(4, len(data)):
            crc += data[i]
            crc &= 0xff
        data += bytes([crc])

        data += bytes([0x16])  #stopSign;                // 0x16

        assert(len(data) == LENGTH+6)
        return data



def updateCounterFromRemoteUrl(amis):
    try:
        request = urllib.request.Request(opts.remoteUrl)
        if opts.remoteUsername or opts.remotePassword:
            base64string = base64.b64encode(bytes('%s:%s' % (opts.remoteUsername, opts.remotePassword),'ascii'))
            request.add_header("Authorization", "Basic %s" % base64string.decode('utf-8'))

        with urllib.request.urlopen(request, timeout=2.5) as url:
            result = url.read()
            #print(result)
            resulttext = result.decode(encoding='utf-8',errors='ignore')
            #print(resulttext)
            data = json.loads(resulttext)
            #print(data)
            amis.c1_8_0 = data["1.8.0"]
            amis.c2_8_0 = data["2.8.0"]
            amis.c3_8_1 = data["3.8.1"]
            amis.c4_8_1 = data["4.8.1"]
            amis.c1_7_0 = data["1.7.0"]
            amis.c2_7_0 = data["2.7.0"]
            amis.c3_7_0 = data["3.7.0"]
            amis.c4_7_0 = data["4.7.0"]
            amis.c1_128_0 = data["1.128.0"]
            amis.timeStamp = data["time"]
            return True
    except Exception as e:
        log(e)
        log("ERROR: Got no valid remote data")
        return False
    log("ERROR: Got no valid remote data")
    return False


def run():
    ser = AmisSerial(opts.port)
    amis = AmisMeter(opts.key, opts.serialNumber, opts.timeoffset)

    # initialize counters
    for i in range(len(amis.counters)):
        amis.counters[i] = i + 10

    while True:
        if amis.serialNumber:
            # we're waiting for requesting serialnumber 10 secs
            ser.trySendSerialnumber(amis.serialNumber, 10)
        if opts.remoteUrl:
            ser.trySendCounterData(amis, updateCounterFromRemoteUrl)
        else:
            ser.trySendCounterData(amis, None)

    ser.close()
    ser = None


def displayUsage(argv):
    print("%s [options]" % os.path.basename(argv[0]))
    print()
    print("Minimal simulation of a 'Siemens TD-3511/TD-3512' smartmeter")
    print("used for development of AMIS-Reader.")
    print("This program can act as proxy or stand-alone smartmeter.")
    print()
    print("Options:")
    print("    -h, --help             Display this page")
    print("    -q, --quiet            Decrease loglevel")
    print("    -v, --verbose          Increase loglevel")
    print("    -p, --port             Use this serial interface for communication")
    print("    -k, --key              Encryption is done with this key")
    print("    -r, --remote-url       Specify a remote rest url of an existing Amis-Reader")
    print("        --remote-username  Specify a username for remote URL")
    print("        --remote-password  Specify a password for remote URL")
    print("    -s, --serialnumber     Specify serialnumber of counter")
    print("    -t, --timeoffset       Specify an offset for the timestamp")


def main(argv):
    try: assert 0
    except AssertionError: pass
    else: raise Exception("fatal error - assertions not enabled")
    shortopts, longopts = "hqvp:t:r:s:k:", [
        "timeoffset=", "serialnumber=", "port=", "key=",
        "remote-url=", "remote-username=",  "remote-password=",
        "quiet", "verbose", "help",
    ]
    xopts, args = getopt.gnu_getopt(argv[1:], shortopts, longopts)
    for opt, optarg in xopts:
        if opt in ["-q", "--quiet"]:
            opts.verbose = opts.verbose - 1
        elif opt in ["-v", "--verbose"]:
            opts.verbose = opts.verbose + 1
        elif opt in ["-h", "--help"]:
            displayUsage(argv)
            return 0
        elif opt in ["--timeoffset", "-t"]:
            opts.timeoffset = int(optarg)
        elif opt in ["--port", "-p"]:
            opts.port = optarg
        elif opt in ["--remote-url", "-r"]:
            opts.remoteUrl = optarg
        elif opt in ["--remote-username"]:
            opts.remoteUsername = optarg
        elif opt in ["--remote-password"]:
            opts.remotePassword = optarg
        elif opt in ["--serialnumber", "-s"]:
            opts.serialNumber = optarg
        elif opt in ["--key", "-k"]:
            opts.key = optarg
        else:
            assert 0, ("getopt problem:", opt, optarg, xopts, args)

    run()


if __name__ == "__main__":
    sys.exit(main(sys.argv))

