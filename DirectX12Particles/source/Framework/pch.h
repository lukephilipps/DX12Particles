#pragma once

// DirectX 12 headers
#include <d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <wrl.h>
using namespace Microsoft::WRL;

// min and max conflict with member functions, only use the ones from <algorithm>
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// WinHelp is deprecated
#define NOHELP

#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <cstdint>
#include <exception>

#include "Helpers.h"
#include "Events.h"
#include "HighResolutionClock.h"
#include "../../resource.h"

#define MAX_NAME_STRING 256
#define HInstance() GetModuleHandle(NULL)