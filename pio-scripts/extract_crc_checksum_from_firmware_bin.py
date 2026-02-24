# Copyright (C) 2025 Stefan Oberhumer (stefan@obssys.com) and others

## vim:set ts=4 sw=4 et: -*- coding: utf-8 -*-

# Extract CRC32 checksum from a firmware.bin file, write it as readable hex into a file and print it
#


import os, sys, struct


Import("env")

def extract_crc32_from_firmware_bin(target, source, env):
    binfile = target[0].get_path()
    fp = open(binfile, "rb")
    fp.seek(4096 + 16 + 4, 0)
    crc = struct.unpack("<I", fp.read(4))[0]
    fp.close()

    # write it to an extra file (with cr/lf)
    #firmwareBinCrc = os.path.join(os.path.dirname(binfile), "firmware.bin.crc32")
    fp = open(binfile + ".crc32", "wb")
    fp.write(b"0x%08x\r\n" % crc)
    fp.close()

    # print it
    print("Firmware CRC32 of %s: 0x%08x" % (binfile, crc))

firmwareBin = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
env.AddPostAction(firmwareBin, extract_crc32_from_firmware_bin)
