#pragma once

// Inside Unreal, PALEOTRENCHCORE_API is supplied by the build tool
// (dllexport / dllimport). In the standalone test build it does not exist.
#if defined(PALEOTRENCHCORE_API)
#define PT_API PALEOTRENCHCORE_API
#else
#define PT_API
#endif
