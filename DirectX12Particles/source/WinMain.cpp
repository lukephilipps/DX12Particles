#include "pch.h"

WCHAR		WindowClass[MAX_NAME_STRING];
WCHAR		WindowTitle[MAX_NAME_STRING];
INT			WindowWidth;
INT			WindowHeight;

LRESULT CALLBACK WindowProcess(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wparam, lparam);
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	/* Initialize Global Variables */
	wcscpy_s(WindowClass, TEXT("DX12Particles"));
	wcscpy_s(WindowTitle, TEXT("MADE A WINDOW SICK !!!!!!!!!!!"));
	WindowWidth = 1366;
	WindowHeight = 768;

	/* Create Window Class */
	/* Note that this is making the window, DX12 is not initialized yet */
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;

	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

	wcex.hIcon = LoadIcon(0, IDI_APPLICATION);
	wcex.hIconSm = LoadIcon(0, IDI_APPLICATION);

	wcex.lpszClassName = WindowClass;
	wcex.lpszMenuName = nullptr;

	wcex.hInstance = HInstance();

	wcex.lpfnWndProc = WindowProcess;

	RegisterClassEx(&wcex);

	/* Create and display the window */
	HWND hWnd = CreateWindow(WindowClass, WindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, WindowWidth, WindowHeight, nullptr, nullptr, HInstance(), nullptr);

	// Handle hWnd error
	if (!hWnd)
	{
		MessageBox(0, L"Failed to Create Window!", 0, 0);
		return 0;
	}

	ShowWindow(hWnd, SW_SHOW);

	/* Listen for message events */
	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		// If there are window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}