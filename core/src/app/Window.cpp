#include "Window.h"

#include <tchar.h>

#include "Event.h"
#include "imgui.h"
#include "core/Log.h"
#include "rend/Device.h"

Window* sWindow = nullptr;

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
   if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
      return true;

   if (!sWindow || !sWindow->eventCallback) {
      return ::DefWindowProc(hWnd, msg, wParam, lParam);;
   }

   switch (msg) {
      case WM_SIZE: if (wParam != SIZE_MINIMIZED) {
         WindowResizeEvent e{ int2{(UINT)LOWORD(lParam), (UINT)HIWORD(lParam)} };
         sWindow->eventCallback(e);
         }
         return 0;
      case WM_SYSCOMMAND: if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
         break;
      case WM_DESTROY: ::PostQuitMessage(0);
         return 0;
      case WM_DPICHANGED: if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top,
                           suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
         }
         break;
   }
   return ::DefWindowProc(hWnd, msg, wParam, lParam);
}


Window::Window(int2 size) {
   //ImGui_ImplWin32_EnableDpiAwareness();
   WNDCLASSEX wc = {
      sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
      _T("ImGui Example"), NULL
   };
   ::RegisterClassEx(&wc);
   hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100,
                              size.x, size.y, NULL, NULL, wc.hInstance, NULL);

   sWindow = this;

   // Show the window
   ::ShowWindow(hwnd, SW_SHOWDEFAULT);
   ::UpdateWindow(hwnd);

   INFO("Window inited with size {}x{}", size.x, size.y);
}

Window::~Window() {
   ::DestroyWindow(hwnd);
   ::UnregisterClass(wc.lpszClassName, wc.hInstance);
}

void Window::Update() {
   MSG msg;
   while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);

      if (msg.message == WM_QUIT) {
         AppQuitEvent e;
         eventCallback(e);
      }
   }
}
