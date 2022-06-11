// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include <d3d11.h>
#include <memory>
#include <tchar.h>

#include "core.h"
#include "app/Window.h"
#include "core/Ref.h"
#include "rend/Device.h"
#include "gui/ImGuiLayer.h"
#include "rend/Texture2D.h"


class Application {
public:
   virtual ~Application() = default;

   virtual void OnInit() {
   }

   virtual void OnTerm() {
   }
};

class CommandList {
public:
   CommandList(ID3D11DeviceContext* pContext) : pContext(pContext) {
      
   }

   CommandList() {
      sDevice->g_pd3dDevice->CreateDeferredContext(0, &pContext);
   }


   void SetRenderTargets(Texture2D* rt, Texture2D* depth = nullptr) {
      int nRtvs = rt ? 1 : 0;
      ID3D11RenderTargetView** rtv = rt ? &rt->rtv : nullptr;
      ID3D11DepthStencilView* dsv = depth ? depth->dsv : nullptr;

      pContext->OMSetRenderTargets(nRtvs, rtv, dsv);
   }

   void ClearRenderTarget(Texture2D& rt, const vec4& color) {
      pContext->ClearRenderTargetView(rt.rtv, &color.x);
   }


   ID3D11CommandList* GetD3DCommandList() {
      return nullptr;
   }


   ID3D11DeviceContext* pContext{};
};


// Main code
int main(int, char**) {
   sWindow = new Window();

   sDevice = new Device();
   if (!sDevice->created) {
      delete sDevice;
      delete sWindow;
      return 1;
   }

   CommandList cmd{sDevice->g_pd3dDeviceContext};

   Scope<ImGuiLayer> imguiLayer = std::make_unique<ImGuiLayer>();
   // ImGuiLayer* imguiLayer = new ImGuiLayer{};

   // Our state
   bool show_demo_window = true;
   bool show_another_window = false;
   ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

   // Main loop
   bool done = false;
   while (!done) {
      // Poll and handle messages (inputs, window resize, etc.)
      // See the WndProc() function below for our to dispatch events to the Win32 backend.
      MSG msg;
      while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
         ::TranslateMessage(&msg);
         ::DispatchMessage(&msg);
         if (msg.message == WM_QUIT)
            done = true;
      }
      if (done)
         break;

      imguiLayer->NewFrame();

      // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
      if (show_demo_window)
         ImGui::ShowDemoWindow(&show_demo_window);

      // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
      {
         static float f = 0.0f;
         static int counter = 0;

         ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

         ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
         ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
         ImGui::Checkbox("Another Window", &show_another_window);

         ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
         ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

         if (ImGui::Button("Button"))
            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
         ImGui::SameLine();
         ImGui::Text("counter = %d", counter);

         CoreUI();
         core::CoreUI2();

         ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                     ImGui::GetIO().Framerate);
         ImGui::End();
      }

      // 3. Show another simple window.
      if (show_another_window) {
         ImGui::Begin("Another Window", &show_another_window);
         // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
         ImGui::Text("Hello from another window!");
         if (ImGui::Button("Close Me"))
            show_another_window = false;
         ImGui::End();
      }

      const float clear_color_with_alpha[4] = {
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
               };
      vec4 clearColor = {
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
      };;

      // todo:
      // sDevice->g_pd3dDeviceContext->OMSetRenderTargets(1, &sDevice->backBuffer->rtv, NULL);
      // sDevice->g_pd3dDeviceContext->ClearRenderTargetView(sDevice->backBuffer->rtv, clear_color_with_alpha);

      
      cmd.ClearRenderTarget(*sDevice->backBuffer, clearColor);
      cmd.SetRenderTargets(sDevice->backBuffer);


      // ID3D11DeviceContext* context{};
      // sDevice->g_pd3dDevice->CreateDeferredContext(0, &context);
      //
      // context->OMSetRenderTargets(1, &sDevice->backBuffer->rtv, NULL);
      // context->ClearRenderTargetView(sDevice->backBuffer->rtv, clear_color_with_alpha);
      // ID3D11CommandList* commandList;
      // context->FinishCommandList(true, &commandList);
      //
      // sDevice->g_pd3dDeviceContext->ExecuteCommandList(commandList, true);
      //
      // commandList->Release();
      // context->Release();


      sDevice->g_pd3dDeviceContext->OMSetRenderTargets(1, &sDevice->backBuffer->rtv, NULL);
      imguiLayer->EndFrame();

      // todo:
      sDevice->g_pSwapChain->Present(1, 0); // Present with vsync
      //g_pSwapChain->Present(0, 0); // Present without vsync
   }

   imguiLayer = nullptr;
   delete sDevice;
   delete sWindow;

   return 0;
}
