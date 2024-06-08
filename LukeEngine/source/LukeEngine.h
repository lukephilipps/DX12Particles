#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#ifdef BUILD_DLL
	#define LUKE_API __declspec(dllexport)
#else
	#define LUKE_API __declspec(dllimport)
#endif