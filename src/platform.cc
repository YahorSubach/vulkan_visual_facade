#include "platform.h"

#include<atomic>

namespace vkvf::platform
{
#ifdef WIN32
	using Window = HWND;


	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		//case WM_PAINT:
		//{
		//	//PAINTSTRUCT ps;
		//	//HDC hdc = BeginPaint(hwnd, &ps);



		//	//FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

		//	//EndPaint(hwnd, &ps);
		//}
		return 0;
		}

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	std::thread window_thread;
	std::atomic_bool window_created;
	Window window;
	bool window_closed = false;
	
	Window CreatePlatformWindow(InitParam param)
	{
		window_created = false;

		window_thread = (std::thread([param]() {

			const wchar_t CLASS_NAME[] = L"VKVF WINDOW CLASS";

			WNDCLASSW wc = { };

			wc.lpfnWndProc = WindowProc;
			wc.hInstance = param;
			wc.lpszClassName = CLASS_NAME;


			RegisterClassW(&wc);

			window = CreateWindowExW(
				0,                              // Optional window styles.
				CLASS_NAME,                     // Window class
				L"Vulkan Visual Facade",    // Window text
				WS_OVERLAPPEDWINDOW,            // Window style

				// Size and position
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

				NULL,       // Parent window    
				NULL,       // Menu
				param,  // Instance handle
				NULL        // Additional application data
			);

			window_created = true;

			if (window)
			{
				ShowWindow(window, 1);
			}

			MSG msg = {};

			while (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			window_closed = true;

			}));

		while (!window_created)
		{
			Sleep(100);
		}

		return window;
	}

	void DestroyPlatformWindow(Window window)
	{
		DestroyWindow(window);
	}



	void ShowWindow(Window window)
	{

	}

	bool IsWindowClosed(Window window)
	{
		return window_closed;
	}


	const std::vector<const char*>& GetRequiredExtensions()
	{
		static const std::vector<const char*> extensions{ "VK_KHR_surface", "VK_KHR_win32_surface"};
		return extensions;
	}

	bool GetPhysicalDevicePresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
	{
		return vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
	}

#endif
}
