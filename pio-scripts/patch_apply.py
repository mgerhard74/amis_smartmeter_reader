# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Thomas Basler and others
#
import os, sys
import subprocess
import re

Import("env")


class globs:
    honorErrors = True
    errCnt = 0
    verbose = 1


def getPatchPath(env):
    patchList = []
    for patch in env.GetProjectOption('custom_patches').split(","):
        patchList.append(os.path.join(env["PROJECT_DIR"], "patches", patch))
    return patchList

def is_tool(name):
    """Check whether `name` is on PATH and marked as executable."""

    # from whichcraft import which
    from shutil import which

    return which(name) is not None

def replaceInFile(in_file, out_file, text, subs, flags=0):
    """
        Function for replacing content for the given file
        Taken from https://www.studytonight.com/python-howtos/search-and-replace-a-text-in-a-file-in-python
    """
    if os.path.exists(in_file):
        with open(in_file, "rb") as infile:
            with open(out_file, "wb") as outfile:
                #read the file contents
                file_contents = infile.read()
                text_pattern = re.compile(re.escape(text), flags)
                file_contents = text_pattern.sub(subs, file_contents.decode('utf-8'))
                outfile.seek(0)
                outfile.truncate()
                outfile.write(file_contents.encode())


def strInFile(filename, strToSearch):
    if (not os.path.exists(filename)):
        return False
    infile = open(filename, "rb")
    content = infile.read().decode('utf-8')
    infile.close()
    return content.find(strToSearch) != -1


def printSubprocessResult(result):
    if (result.stdout):
        print(result.stdout)
    if (result.stderr):
        print(result.stderr)


def isPatchApplied(gitExtraOptions, patchFilename):
    gitExec = ['git', 'apply'] + gitExtraOptions + ['--reverse', '--check', patchFilename]
    if globs.verbose >= 3:
        print("Running '%s'" % " ".join(gitExec))
    process = subprocess.run(gitExec, capture_output=True) # subprocess.DEVNULL
    if globs.verbose >= 3:
        printSubprocessResult(process)
    if (process.returncode == 0):
        return True
    return False


def applyPatch(gitExtraOptions, patchFilename):
    gitExec = ['git', 'apply'] + gitExtraOptions + [patchFilename]
    if globs.verbose >= 3:
        print("Running '%s'" % " ".join(gitExec))
    process = subprocess.run(gitExec, capture_output=True) # subprocess.DEVNULL
    if globs.verbose >= 3:
        printSubprocessResult(process)
    if (process.returncode == 0):
        return True
    if globs.verbose < 3:
        printSubprocessResult(process)
    return False


def unapplyPatch(gitExtraOptions, patchFilename):
    gitExec = ['git', 'apply', '--reverse'] + gitExtraOptions + [patchFilename]
    if globs.verbose >= 3:
        print("Running '%s'" % " ".join(gitExec))
    process = subprocess.run(gitExec, capture_output=True) # subprocess.DEVNULL
    if globs.verbose >= 3:
        printSubprocessResult(process)
    if (process.returncode == 0):
        return True
    if globs.verbose < 3:
        printSubprocessResult(process)
    return False


def main():
    if (env.GetProjectOption('custom_patches', '') == ''):
        print('No custom_patches specified')
        return 0

    if (not is_tool('git')):
        print('Git not found. Will not apply custom patches!')
        if (globs.honorErrors):
            sys.exit(10)
        return 0

    directories = getPatchPath(env)
    for directory in directories:
        if (not os.path.isdir(directory)):
            print('Patch directory not found: ' + directory)
            if (globs.honorErrors):
                sys.exit(11)
            return 0

        for filename in os.listdir(directory):
            if (not filename.endswith('.patch')):
                continue

            origPatchFilename = os.path.join(directory, filename)
            preparedPatchFilename = origPatchFilename + "." + env['PIOENV'] + '.prepare'

            gitExtraOptions = []
            # gitExtraOptions = ['--ignore-space-change'] # not needed as patch should apply exactly
            # gitExtraOptions = ['--whitespace=error-all'] # try beeing very strict
            # TODO(anybody) try figuring out how force git patching exacly (including check lineending win/linux)
            if strInFile(origPatchFilename, "$$$env$$$"):
                replaceInFile(origPatchFilename, preparedPatchFilename, '$$$env$$$', env['PIOENV'] )
            else:
                print("error: Invalid patch format '%s'" % origPatchFilename)
                globs.errCnt += 1
                continue

            if globs.verbose >= 2:
                # print the adapted .patch file
                print("Adapted patch file: '%s'" % preparedPatchFilename)
                print(open(preparedPatchFilename, "rt").read())

            print('Working on patch: ' + origPatchFilename + '... ', end='')
            if globs.verbose >= 3:
                print()

            # Check if patch was already applied
            if (isPatchApplied(gitExtraOptions, preparedPatchFilename)):
                os.remove(preparedPatchFilename)
                if globs.verbose >= 1:
                    print('already applied')
                continue

            # Apply patch
            if (applyPatch(gitExtraOptions, preparedPatchFilename)):
                print('applied')
            else:
                print('failed')
                globs.errCnt += 1
            os.remove(preparedPatchFilename)

    if (globs.honorErrors and globs.errCnt):
        sys.exit(12)
    return 0


# Variablen und Werte
# print(env.Dump())
#
# Methoden/Attribute
# print(dir(env))

main()
