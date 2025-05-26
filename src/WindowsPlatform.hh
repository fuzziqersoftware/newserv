#pragma once

// <windows.h> defines a lot of annoying things like ERROR and DELETE, which
// we use as names in enum classes, so we wrap <windows.h> with this header
// that undoes some of these annoying things.

#include <phosg/Platform.hh>

#ifdef PHOSG_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef DELETE
#undef ERROR
#undef PASSTHROUGH
#endif
