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

        for file in os.listdir(directory):
            if (not file.endswith('.patch')):
                continue

            fullPath = os.path.join(directory, file)
            preparePath = fullPath + "." + env['PIOENV'].strip() + '.prepare'
            replaceInFile(fullPath, preparePath, '$$$env$$$', env['PIOENV'])
            print('Working on patch: ' + fullPath + '... ', end='')

            # Check if patch was already applied
            process = subprocess.run(
                ['git', 'apply', '--reverse', '--check', '--ignore-space-change', preparePath],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            if (process.returncode == 0):
                print('already applied')
                os.remove(preparePath)
                continue

            # Apply patch
            process = subprocess.run(['git', 'apply', '--ignore-space-change', preparePath])
            if (process.returncode == 0):
                print('applied')
            else:
                print('failed')
                globs.errCnt += 1

            os.remove(preparePath)

    if (globs.honorErrors and globs.errCnt):
        sys.exit(12)
    return 0

main()
