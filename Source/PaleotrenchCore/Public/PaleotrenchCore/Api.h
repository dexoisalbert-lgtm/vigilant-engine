#pragma once

// Inside Unreal, PALEOTRENCHCORE_API is supplied by the build tool as
// DLLEXPORT / DLLIMPORT. Those two names normally come from Unreal's platform
// headers, which engine-free files must not include, so define them here when
// they are missing. In the standalone test build none of this exists.
#if defined(PALEOTRENCHCORE_API)
	#if !defined(DLLEXPORT)
		#if defined(_MSC_VER)
			#define DLLEXPORT __declspec(dllexport)
		#else
			#define DLLEXPORT __attribute__((visibility("default")))
		#endif
	#endif
	#if !defined(DLLIMPORT)
		#if defined(_MSC_VER)
			#define DLLIMPORT __declspec(dllimport)
		#else
			#define DLLIMPORT __attribute__((visibility("default")))
		#endif
	#endif
	#define PT_API PALEOTRENCHCORE_API
#else
	#define PT_API
#endif
