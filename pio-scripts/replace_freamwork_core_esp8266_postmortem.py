# Copyright (C) 2025 Stefan Oberhumer (stefan@obssys.com) and others

## vim:set ts=4 sw=4 et: -*- coding: utf-8 -*-

# Replace files while compiling with different file
#
# See:
#   https://docs.platformio.org/en/latest/scripting/middlewares.html
#   https://docs.platformio.org/en/latest/scripting/middlewares.html#scripting-middlewares


import os, sys
from SCons.Script import Import


#selfname = os.path.basename(__file__)
selfname = "pio-scripts/change_files.py"

Import("env")

# get dynamic path to the framework
# This should find the folder ~/.platformio/packages/framework-arduinoespressif8266
platform = env.PioPlatform()
#print(env['PIOFRAMEWORK']          ['arduino']
#print(env['PIOPLATFORM']           'espressif8266'
packageName = "framework-" + env['PIOFRAMEWORK'][0] + env['PIOPLATFORM']
framework_dir = platform.get_package_dir(packageName)

if not framework_dir:
    print(f"{selfname}: --- ERROR: Framework directory ({packageName}) not found! ---")
    sys.exit(10)


# Helper to replace some vars in a filename
def substFilename(f):
    f = f.replace('$framework_dir', framework_dir)
    f = f.replace('$BUILD_DIR', env['BUILD_DIR'])
    f = f.replace('$PROJECT_SRC_DIR', env['PROJECT_SRC_DIR'])
    f = f.replace('$PROJECT_DIR', env['PROJECT_DIR'])
    f = f.replace('$PROJECT_BUILD_DIR', env['PROJECT_BUILD_DIR'])
    f = f.replace('$PIOENV', env['PIOENV'])
    return f


# Replaces vars and normalize filenames. Create and register middleware to change source file
def replaceFile1(origSource, newSource, targetfile):
    # replace $$$xxx$$$ vars
    origSource = substFilename(origSource)
    newSource = substFilename(newSource)
    targetfile = substFilename(targetfile)

    # normalize (behebt Probleme mit Backslashes unter Windows)
    origSource = os.path.normpath(origSource)
    newSource = os.path.normpath(newSource)
    targetfile = os.path.normpath(targetfile)

    # Define the middleware (this replaces the file which gets compiled)
    def changeSrcFile(node):
        if False:
            print(node)
            #print(dir(node))
            print(node.get_path())
            #print(node.srcdir)
            #print(node.target_from_source)
            print(node.srcnode)
            print(node.sources)
            #print(node.variant_dir)
            #node.variant_dir = "/"
            sys.exit(99)

        print(f"{selfname}: SUCCESS: Replaced '{origSource}' with '{newSource}': Output to '{targetfile}'")
        #print(node.get_path())
        return env.Object(
            target=targetfile,
            source=newSource
        )
        # return None # ignoriere file (do not compile it)

    if not os.path.isfile(origSource):
        print(f"{selfname}: --- ERROR: File '{origSource}' does not exists! ---")
        sys.exit(3)

    if not os.path.isfile(newSource):
        print(f"{selfname}: --- ERROR: File '{newSource}' does not exists! ---")
        sys.exit(4)

    print(f"{selfname}: Prepare replacing '{origSource}' with '{newSource}': Output to '{targetfile}'")

    # register middleware (register the replace callback function)
    env.AddBuildMiddleware(changeSrcFile, origSource)



# Replace .platformio/packages/framework-arduinoespressif8266/cores/esp8266/core_esp8266_postmortem.cpp with our own
replaceFile1(os.path.join("$framework_dir", "cores", "esp8266", "core_esp8266_postmortem.cpp"),
            os.path.join("$PROJECT_DIR", "framework", "replacement", "cores", "esp8266", "core_esp8266_postmortem.cpp"),
            os.path.join("$BUILD_DIR", "FrameworkArduino", "core_esp8266_postmortem.cpp" + env['OBJSUFFIX'])
            )


#print(env.Dump())
#print(env['BUILD_DIR'])            '$PROJECT_BUILD_DIR/$PIOENV'
#print(env['PIOENV'])               'esp12e' or 'esp12e_debug'
#print(env['PROJECT_BUILD_DIR'])    '........./amis_smartmeter_reader/.pio/build'
#print(env['PROJECT_CORE_DIR'])     '...../.platformio',
#print(env['PROJECT_DIR'])          '........./amis_smartmeter_reader'
#print(env['PROJECT_SRC_DIR'])      '........./amis_smartmeter_reader/src'
#print(env['PIOFRAMEWORK']          ['arduino']
#print(env['PIOPLATFORM']           'espressif8266'
#sys.exit(1)


# pio run -t envdump
