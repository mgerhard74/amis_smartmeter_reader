#pragma once


void Exception_DumpLastCrashToFile();
void Exception_Raise(unsigned int no);
void Exception_RemoveAllDumps();
void Exception_InstallPostmortem(int target);



/* vim:set ts=4 et: */
