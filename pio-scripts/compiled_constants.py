# Copyright (C) 2025 Stefan Oberhumer (stefan@obssys.com)

## vim:set ts=4 sw=4 et: -*- coding: utf-8 -*-

# Create ..../build/__compiled_constants.c which contains some compile time constants.

# 1.) Date Time
# It seems there are missing "__ DATE __" and "__ TIME __" compiler runtime defines within platformio!
# So we've to build our "Compiled on" string ourself

# 2.) GIT informations
# Also enable getting some information from current git (branch / revision)


class opts:
    generate__COMPILED_DATE_TIME_UTC_STR__ = True

    generate__COMPILED_GIT_HASH__ = True
    generate__COMPILED_GIT_BRANCH__ = True

    write_h_file = False
    write_c_file = True

    use_defines = False



import hashlib, os
from datetime import datetime, timezone

Import("env")

# Import porcelain from dulwich and if not available: install dulwich
import pkg_resources
required_pkgs = {'dulwich'}
installed_pkgs = {pkg.key for pkg in pkg_resources.working_set}
missing_pkgs = required_pkgs - installed_pkgs
if missing_pkgs:
    env.Execute('"$PYTHONEXE" -m pip install dulwich')
try:
    from dulwich import porcelain
except:
    pass

def gitGetBuildVersion():
    try:
        # to get a value here you must create an "Annotated Tag" in git
        # eg: ' git tag -a v1.29 -m "version 1.29" '
        build_version = porcelain.describe('.')  # '.' refers to the repository root dir
    except:
        build_version = "g0000000"
    print ("Firmware Revision: " + build_version)
    return build_version


def gitGetBuildBranch():
    try:
        branch_name = porcelain.active_branch('.').decode('utf-8')  # '.' refers to the repository root dir
        #print (dir(porcelain))
        #import sys
        #sys.exit(10)
    except Exception as err:
        #branch_name = "master"
        branch_name = "main"
    print("Firmware Branch: " + branch_name)
    return branch_name

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


targetfile_h = os.path.join(env.subst("$BUILD_DIR"), "__compiled_constants.h")
targetfile_c = os.path.join(env.subst("$BUILD_DIR"), "__compiled_constants.c")

lines_h = ""
lines_c = ""

lines_h += "/* generated file - do not edit */\n"
lines_h += "#pragma once\n"
lines_c += "/* generated file - do not edit */\n"

if opts.generate__COMPILED_DATE_TIME_UTC_STR__:
    # Add the corrent date and time as string in UTC timezone
    now = datetime.now(tz=timezone.utc)
    COMPILED_DATE_TIME_UTC_STR = now.strftime("%Y/%m/%d %H:%M:%S")
    if opts.use_defines:
        lines_h += '#define __COMPILED_DATE_TIME_UTC_STR__ "%s"\n' % (COMPILED_DATE_TIME_UTC_STR)
    else:
        lines_h += 'extern const char *__COMPILED_DATE_TIME_UTC_STR__;\n'
        lines_c += 'const char *__COMPILED_DATE_TIME_UTC_STR__ = "%s";\n' % (COMPILED_DATE_TIME_UTC_STR)

if opts.generate__COMPILED_GIT_HASH__:
    if opts.use_defines:
        lines_h += '#define __COMPILED_GIT_HASH__ "%s"\n' % (gitGetBuildVersion())
    else:
        lines_h += 'extern const char *__COMPILED_GIT_HASH__;\n'
        lines_c += 'const char *__COMPILED_GIT_HASH__ = "%s";\n' % (gitGetBuildVersion())

if opts.generate__COMPILED_GIT_BRANCH__:
    if opts.use_defines:
        lines_h += '#define __COMPILED_GIT_BRANCH__ "%s"\n' % (gitGetBuildBranch())
    else:
        lines_h += 'extern const char *__COMPILED_GIT_BRANCH__;\n'
        lines_c += 'const char *__COMPILED_GIT_BRANCH__ = "%s";\n' % (gitGetBuildBranch())

if lines_h and opts.write_h_file:
    updateFileIfChanged(targetfile_h, bytes(lines_h, "utf-8"))
    env.AppendUnique(CCFLAGS=["-I", os.path.dirname(targetfile_h)])
if lines_c and opts.write_c_file:
    updateFileIfChanged(targetfile_c, bytes(lines_c, "utf-8"))
    # Add the created file to the buildfiles - platformio knows how to handle *.c files
    env.AppendUnique(PIOBUILDFILES=[targetfile_c])
