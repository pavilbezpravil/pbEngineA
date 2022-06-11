#include "ImGuiLayer.h"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "app/Window.h"
#include "rend/Device.h"

ImGuiLayer::ImGuiLayer() {
   // Setup Dear ImGui context
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO& io = ImGui::GetIO();
   (void)io;
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
   io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
   //io.ConfigViewportsNoAutoMerge = true;
   //io.ConfigViewportsNoTaskBarIcon = true;
   //io.ConfigViewportsNoDefaultParent = true;
   //io.ConfigDockingAlwaysTabBar = true;
   //io.ConfigDockingTransparentPayload = true;
   //io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: Experimental. THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
   //io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI: Experimental.

   // Setup Dear ImGui style
   ImGui::StyleColorsDark();
   //ImGui::StyleColorsClassic();

   // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
   ImGuiStyle& style = ImGui::GetStyle();
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
   }

   // Setup Platform/Renderer backends
   ImGui_ImplWin32_Init(sWindow->hwnd);
   ImGui_ImplDX11_Init(sDevice->g_pd3dDevice, sDevice->g_pd3dDeviceContext);

   // Load Fonts
   // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
   // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
   // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
   // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
   // - Read 'docs/FONTS.md' for more instructions and details.
   // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
   //io.Fonts->AddFontDefault();
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
   //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
   //IM_ASSERT(font != NULL);
}

ImGuiLayer::~ImGuiLayer() {
   // Cleanup
   ImGui_ImplDX11_Shutdown();
   ImGui_ImplWin32_Shutdown();
   ImGui::DestroyContext();
}

void ImGuiLayer::NewFrame() {
   // Start the Dear ImGui frame
   ImGui_ImplDX11_NewFrame();
   ImGui_ImplWin32_NewFrame();
   ImGui::NewFrame();
}

void ImGuiLayer::EndFrame() {
   // Rendering
   ImGui::Render();

   ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

   ImGuiIO& io = ImGui::GetIO();
   // Update and Render additional Platform Windows
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
   }
}
