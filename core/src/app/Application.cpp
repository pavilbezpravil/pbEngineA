#include "Application.h"

#include "core/Common.h"
#include "rend/Device.h"
#include "rend/CommandList.h"
#include "Window.h"
#include "gui/ImGuiLayer.h"


Application* sApplication = nullptr;


void Application::OnInit() {
   new Window();

   new Device();
   if (!sDevice->created) {
      return;
   }

   sWindow->eventCallback = std::bind(&Application::OnEvent, this, std::placeholders::_1);

   running = true;
}

void Application::OnTerm() {
   SAFE_DELETE(sDevice);
   SAFE_DELETE(sWindow);
}

void Application::OnEvent(Event& e) {
   if (e.GetEvent<AppQuitEvent>()) {
      running = false;
   }
   if (auto* windowResize = e.GetEvent<WindowResizeEvent>()) {
      sDevice->Resize(windowResize->size);
   }
}

void Application::Run() {
   if (!running) {
      return;
   }

   CommandList cmd{ sDevice->g_pd3dDeviceContext };

   Scope<ImGuiLayer> imguiLayer = std::make_unique<ImGuiLayer>();
   // ImGuiLayer* imguiLayer = new ImGuiLayer{};


   // Our state
   bool show_demo_window = true;
   bool show_another_window = false;
   ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

   // Main loop
   bool done = false;
   while (running) {
      sWindow->Update();
      if (!running) {
         break;
      }

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
      };

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
}
