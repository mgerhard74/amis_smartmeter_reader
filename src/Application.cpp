#include "Application.h"

// Stores parameters which get set just once at startup. They get not saved
ApplicationClass Application;


// Stores parameters which can be changed via external websock calls
// They are not saved in the filesystem so changes are available ONLY till next reboot.
ApplicationRuntimeClass ApplicationRuntime;

/* vim:set ts=4 et: */
