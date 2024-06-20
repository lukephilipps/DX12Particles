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

// min and max conflict with member functions, only use the ones from <algorithm>
#define NOMINMAX

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

#include <algorithm>
#include <cassert>
#include <chrono>

#include "Helpers.h"

#include "../../resource.h"

#define MAX_NAME_STRING 256
#define HInstance() GetModuleHandle(NULL)