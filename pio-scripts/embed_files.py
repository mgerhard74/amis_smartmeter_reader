# Copyright (C) 2025 Stefan Oberhumer (stefan@obssys.com) and others

## vim:set ts=4 sw=4 et: -*- coding: utf-8 -*-


# Embed files into a firmware binary by creating C-files and compile them


import gzip, hashlib, os, sys

Import("env")

def getMd5Digest(data):
    digest = hashlib.md5(data)
    return digest.hexdigest()

def getSha256Digest(data):
    digest = hashlib.sha256(data)
    return digest.hexdigest()

def updateFileIfChanged(filename, content):
    mustUpdate = True
    try:
        fp = open(filename, "rb")
        if fp.read() == content:
            mustUpdate = False
        fp.close()
    except:
        pass
    if mustUpdate:
        fp = open(filename, "wb")
        fp.write(content)
        fp.close()
    return mustUpdate


def filenameToVarname(sourcefilename):
    filename = os.path.relpath(sourcefilename, env.subst("$PROJECT_DIR"))
    r = []
    while True:
        head_tail = os.path.split(filename)
        if not head_tail[1]:
            break
        r.insert(0, head_tail[1])
        filename = head_tail[0]
    r = "_".join(r)
    r = r.replace(".", "_")
    r = r.replace(" ", "_")
    r = r.replace("-", "_")
    #r = r.lower()
    return "__embed_" + r


def getTargetFilename(sourcefilename, extension=".c", prefix="__embed_"):
    #basename = os.path.basename(sourcefilename)
    basename = filenameToVarname(sourcefilename)
    return os.path.join(env.subst("$BUILD_DIR"),  prefix + basename + extension)

def gzipCompress(buffer):
    buffer = gzip.compress(buffer, mtime=0)

    # set gzip-os header byte preventing different results on windows/linux
    if buffer[0] != 0x1f or buffer[1] != 0x8b: # gzip "magic" bytes
        return buffer

    if buffer[9] == 0xff:
        return buffer
    byarray = bytearray(buffer)
    byarray[9] = 0xff # set OS in gzip header to "unknown"
    if True:
        # convert back to the immutable bytearray preventing any writes
        return bytes(byarray)
    else:
        return byarray

def get_PROGMEM_macro(macro_needed):
    lines = ''
    if macro_needed:
        lines += '#ifndef PROGMEM\n'
        lines += '#include <stdio.h> // Needed for PROGMEM macro\n'
        lines += '#ifndef PROGMEM\n'
        lines += '#include <sys/pgmspace.h> // Try pgmspace.h directly\n'
        lines += '#endif\n'
        lines += '#endif\n'
    else:
        lines += '#ifdef PROGMEM\n'
        lines += '#undef PROGMEM\n'
        lines += '#define PROGMEM\n'
        lines += '#endif\n'
        lines += '#ifdef PGM_P\n'
        lines += '#undef PGM_P\n'
        lines += '#define PGM_P const char *\n'
        lines += '#endif\n'

    return lines

def generateCAndHFiles(file_and_opts_lines):
    #join("$PROJECT_DIR", line)
    for l in file_and_opts_lines:
        do_gzip = False
        do_md5 = False
        do_sha256 = False
        sourcefilename = ""
        varname = ""
        add_trailing_zero = False
        usePROGMEM = True
        touch = []
        len_as_define = False

        args = l.split(" ")

        for arg in args:
            if arg in [ "--gzip", "--no-gzip" ]:
                do_gzip = (bool) (arg == "--gzip")
            elif arg in [ "--md5", "--no-md5" ]:
                do_md5 = (bool) (arg == "--md5")
            elif arg in [ "--sha256", "--no-sha256" ]:
                do_sha256 = (bool) (arg == "--sha256")
            elif arg in [ "--lenAsDefine", "--no-lenAsDefine" ]:
                lenAsDefine = (bool) (arg == "--lenAsDefine")
            elif arg == "--addTrailingZero":
                add_trailing_zero = True
            elif arg.startswith("--varname="):
                varname = arg[10:]
            elif arg in [ "--PROGMEM", "--no-PROGMEM" ]:
                usePROGMEM = (bool) (arg == "--PROGMEM")
            elif arg.startswith("--touch="):
                touch.append(arg[8:])
            elif arg == "":
                continue # happens only if more spaces between the filename or options
            else:
                if sourcefilename != "":
                    print("ERROR Invalid option '%s'" % arg)
                    return 2
                sourcefilename += arg

        sourcefilename = os.path.join(env.subst("$PROJECT_DIR"), sourcefilename)

        try:
            f = open(sourcefilename, "rb")
            srcdata = f.read()
        except:
            print("ERROR: File '%s' could not be read!" % sourcefilename)
            return 1
        f.close()

        if not srcdata:
            if add_trailing_zero:
                srcdata = b"00"
            else:
                print("ERROR: File '%s' is empty!" % sourcefilename)
                return 3

        if add_trailing_zero and srcdata[-1] != 0:
            srcdata = bytearray(srcdata)
            srcdata.append(0)
            srcdata = bytes(srcdata)

        if do_gzip:
            srcdata = gzipCompress(srcdata)

        #srcdata = bytes() # disable any embedded files
        srclen = len(srcdata)

        if not varname:
            varname = filenameToVarname(sourcefilename)

        h_filename = getTargetFilename(sourcefilename, ".h", "")
        h_content = ""
        h_content += "/* generated file - do not edit */\n"
        h_content += "#pragma once\n"
        if not len_as_define:
            h_content += "#include <stddef.h>\n"
        h_content += "#include <stdint.h>\n"
        h_content += "extern const uint8_t *%s;\n" % (varname)
        if len_as_define:
            h_content += "#define %s_size %d\n" % (varname, srclen)
        else:
            h_content += "extern const size_t %s_size;\n" % (varname)
        if do_md5:
            h_content += "extern const char *%s_md5;\n" % (varname)
        if do_sha256:
            h_content += "extern const char *%s_sha256;\n" % (varname)
        updateFileIfChanged(h_filename, bytes(h_content, "utf-8"))

        c_filename = getTargetFilename(sourcefilename, ".c", "")
        c_content = ""
        c_content += "/* generated file - do not edit */\n"
        #c_content += '/* #include <stdint.h> */\n'
        c_content += '#include "%s"\n' % os.path.basename(h_filename)
        c_content += get_PROGMEM_macro(usePROGMEM)
        if not len_as_define:
            c_content += "const size_t %s_size = %d;\n" % (varname, srclen)
        if do_md5:
            c_content += "const char *%s_md5 = \"%s\";\n" % (varname, getMd5Digest(srcdata))
        if do_sha256:
            c_content += "const char *%s_sha256 = \"%s\";\n" % (varname, getSha256Digest(srcdata))
        #c_content += "static const uint8_t %s_static[%d] = {\n" % (varname, srclen)
        if not srclen:
            c_content += "const uint8_t *%s = NULL;\n" % (varname)
        else:
            if len_as_define:
                c_content += "static const uint8_t %s_static[%s_size] PROGMEM = {\n" % (varname, varname)
            else:
                c_content += "static const uint8_t %s_static[%d] PROGMEM = {\n" % (varname, srclen)

            i = 0
            l = srclen
            while l > 16:
                c_content += "  /* %6d */ " \
                            "0x%02x, 0x%02x, 0x%02x, 0x%02x,  " \
                            "0x%02x, 0x%02x, 0x%02x, 0x%02x,    " \
                            "0x%02x, 0x%02x, 0x%02x, 0x%02x,  " \
                            "0x%02x, 0x%02x, 0x%02x, 0x%02x,\n"  % ( \
                            i, \
                            srcdata[i+0], srcdata[i+1], srcdata[i+2], srcdata[i+3], \
                            srcdata[i+4], srcdata[i+5], srcdata[i+6], srcdata[i+7], \
                            srcdata[i+8], srcdata[i+9], srcdata[i+10], srcdata[i+11], \
                            srcdata[i+12], srcdata[i+13], srcdata[i+14], srcdata[i+15], \
                            )
                i += 16;  l -= 16

            # last pending bytes
            c_content += "  /* %6d */ " % i
            while l:
                c_content += "0x%02x" % srcdata[i]
                if l > 1:
                    c_content += ", "
                    if i % 4 == 3:
                        c_content += " "
                    if i % 8 == 7:
                        c_content += "  "
                i += 1;  l -= 1
            c_content += "\n"
            c_content += "};\n"
            c_content += "const uint8_t *%s = (const uint8_t *) (PGM_P) &%s_static[0];\n" % (varname, varname)
        updateFileIfChanged(c_filename, bytes(c_content, "utf-8"))

        # Add the directory to the Includes - so we can use #include <....
        # https://docs.platformio.org/en/stable/projectconf/sections/platformio/options/directory/include_dir.html
        env.AppendUnique(CCFLAGS=["-I", os.path.dirname(h_filename)])
        #env.AppendUnique(CCFLAGS=["-include", h_filename])
        #env.AppendUnique(CCFLAGS=["-include %s" % h_filename])
        #print("CCFLAGS=", env["CCFLAGS"])
        #env.AppendUnique(CPPPATH=["-I %s" % os.path.dirname(h_filename)])
        #print("CPPPATH=", env["CPPPATH"])

        # Add the created file to the buildfiles - platformio knows how to handle *.c files
        env.AppendUnique(PIOBUILDFILES=[c_filename])
        #print(env["PIOBUILDFILES"])

        if False:
            # Add files to buildfiles which are indirectly changed (via usage of include)
            for t in touch:
                t = os.path.join(env.subst("$PROJECT_DIR"), t)
                env.AppendUnique(PIOBUILDFILES=[t])
            print(env["PIOBUILDFILES"])
    return 0

for file_type in ["embed_files"]:
    lines = env.GetProjectOption("board_build.%s" % file_type, "").splitlines()
    if not lines:
        continue

    file_and_opts_lines = []
    for line in lines:
        line = line.strip()
        if line:
            file_and_opts_lines += [line]

    r = generateCAndHFiles(file_and_opts_lines)
    if r:
        sys.exit(r)
