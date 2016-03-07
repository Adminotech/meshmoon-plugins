
#pragma once

#ifdef WIN32
#ifdef OPENNI_MODULE_EXPORTS
#define OPENNI_MODULE_API __declspec(dllexport)
#else
#define OPENNI_MODULE_API __declspec(dllimport)
#endif
#else
#define OPENNI_MODULE_API
#endif