#include "Application.h"

#include "core/Common.h"
#include "core/Log.h"
#include "core/Profiler.h"
#include "rend/Device.h"
#include "rend/CommandList.h"
#include "Window.h"
#include "gui/ImGuiLayer.h"


Application* sApplication = nullptr;


void Application::OnInit() {
   Log::Init();

   new Window();

   new Device();
   if (!sDevice->created) {
      return;
   }

   sWindow->eventCallback = std::bind(&Application::OnEvent, this, std::placeholders::_1);

   imguiLayer = new ImGuiLayer();
   PushOverlay(imguiLayer);

   INFO("App init success");
   running = true;
}

void Application::OnTerm() {
   SAFE_DELETE(sDevice);
   SAFE_DELETE(sWindow);
   // Log::Term();
}

void Application::OnEvent(Event& e) {
   if (e.GetEvent<AppQuitEvent>()) {
      INFO("App quit event");
      running = false;
   }
   if (auto* windowResize = e.GetEvent<WindowResizeEvent>()) {
      sDevice->Resize(windowResize->size);
   }

   for (auto it = layerStack.end(); it != layerStack.begin();) {
      (*--it)->OnEvent(e);
      if (e.handled) {
         break;
      }
   }
}


void Application::PushLayer(Layer* layer) {
   layerStack.PushLayer(layer);
   layer->OnAttach();
}


void Application::PushOverlay(Layer* overlay) {
   overlay->OnAttach();
   layerStack.PushOverlay(overlay);
}


void Application::Run() {
   if (!running) {
      return;
   }

   CommandList cmd{ sDevice->g_pd3dDeviceContext };

   ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

   // Main loop
   while (running) {
      OPTICK_FRAME("MainThread");

      float dt = 1.f / 60.f; // todo:
      // debug handle
      if (dt > 1.f) {
         dt = 1.f / 60.f;
      }
      OPTICK_TAG("DeltaTime (ms)", dt * 1000.f);

      for (auto* layer : layerStack) {
         layer->OnUpdate(dt);
      }

      {
         OPTICK_EVENT("OnImGuiRender");
         imguiLayer->NewFrame();

         for (auto* layer : layerStack) {
            layer->OnImGuiRender();
         }

         imguiLayer->EndFrame();
      }

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

      vec4 clearColor = {
      clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
      };

      cmd.ClearRenderTarget(*sDevice->backBuffer, clearColor);
      cmd.SetRenderTargets(sDevice->backBuffer);

      {
         OPTICK_EVENT("ImGui Render");
         // sDevice->g_pd3dDeviceContext->OMSetRenderTargets(1, &sDevice->backBuffer->rtv, NULL);
         imguiLayer->Render();
      }

      {
         OPTICK_EVENT("Swapchain Present");
         // todo:
         sDevice->g_pSwapChain->Present(1, 0); // Present with vsync
         //g_pSwapChain->Present(0, 0); // Present without vsync
      }

      OPTICK_EVENT("Window Update");
      sWindow->Update();
   }

   for (auto* layer : layerStack) {
      layer->OnDetach();
   }

   imguiLayer = nullptr;
}
