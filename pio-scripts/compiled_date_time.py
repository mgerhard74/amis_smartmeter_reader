# Copyright (C) 2024 Stefan Oberhumer (stefan@obssys.com) and others
#
# See LICENCE for more details


# Create ..../build/__build_constants.c which contains some compile time constants.

# It seems there are missing "__ DATE __" and "__ TIME __" compiler runtime defines within platformio!
# So we've to build our "Compiled on" string ourself


import hashlib, os
from datetime import datetime, timezone

Import("env")

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


targetfile = os.path.join(env.subst("$BUILD_DIR"), "__build_constants.c")
lines = ""
lines += "/* generated file - do not edit */\n"

# Add the corrent date and time as string in UTC timezone
now = datetime.now(tz=timezone.utc)
COMPILED_DATE_TIME_UTC_STR = now.strftime("%Y/%m/%d %H:%M:%S")
lines += 'const char *__COMPILED_DATE_TIME_UTC_STR__ = "%s";\n' % (COMPILED_DATE_TIME_UTC_STR)

updateFileIfChanged(targetfile, bytes(lines, "utf-8"))

# Add the created file to the buildfiles - platformio knows how to handle *.c files
env.AppendUnique(PIOBUILDFILES=[targetfile])

