#include "pch.h"
#include "Application.h"

#include "Input.h"
#include "core/Common.h"
#include "core/Log.h"
#include "core/Profiler.h"
#include "rend/Device.h"
#include "rend/CommandList.h"
#include "Window.h"
#include "core/Thread.h"
#include "gui/ImGuiLayer.h"
#include "rend/Shader.h"

namespace pbe {

   Application* sApplication = nullptr;

   void Application::OnInit() {
      Log::Init();

      new Window();

      new Device();
      if (!sDevice->created) {
         return;
      }

      sWindow->eventCallback = [&](Event& event) { OnEvent(event); };

      imguiLayer = new ImGuiLayer();
      PushOverlay(imguiLayer);

      INFO("App init success");
      running = true;
   }

   void Application::OnTerm() {
      layerStack.Clear();

      SAFE_DELETE(sDevice);
      SAFE_DELETE(sWindow);
   }

   void Application::OnEvent(Event& event) {
      Input::OnEvent(event);

      if (event.GetEvent<AppQuitEvent>()) {
         INFO("App quit event");
         running = false;
      }
      // if (event.GetEvent<AppLoseFocusEvent>()) {
      //    INFO("Lose Focus");
      //    focused = false;
      // }
      // if (event.GetEvent<AppGetFocusEvent>()) {
      //    INFO("Get Focus");
      //    focused = true;
      // }

      if (auto* windowResize = event.GetEvent<WindowResizeEvent>()) {
         sDevice->Resize(windowResize->size);
      }

      if (auto* key = event.GetEvent<KeyPressedEvent>()) {
         // INFO("KeyCode: {}", key->keyCode);
         if (Input::IsKeyPressed(VK_CONTROL) && key->keyCode == 'R') {
            ReloadShaders();
            event.handled = true;
         }
      }

      if (!event.handled) {
         for (auto it = layerStack.end(); it != layerStack.begin();) {
            (*--it)->OnEvent(event);
            if (event.handled) {
               break;
            }
         }
      }
   }

   void Application::PushLayer(Layer* layer) {
      layerStack.PushLayer(layer);
   }

   void Application::PushOverlay(Layer* overlay) {
      layerStack.PushOverlay(overlay);
   }

   void Application::Run() {
      if (!running) {
         return;
      }

      for (auto* layer : layerStack) {
         layer->OnAttach();
      }

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      CpuTimer frameTimer{};

      // Main loop
      while (running) {
         OPTICK_FRAME("MainThread");

         if (!focused) {
            OPTICK_EVENT("Sleep On Focused");
            ThreadSleepMs(50);
         }

         float dt = frameTimer.ElapsedMs(true) / 1000.f;
         // INFO("DeltaTime: {}", dt);

         // debug handle
         if (dt > 1.f) {
            dt = 1.f / 60.f;
         }
         OPTICK_TAG("DeltaTime (ms)", dt * 1000.f);

         Profiler::Get().NextFrame();

         Input::OnUpdate(dt);

         for (auto* layer : layerStack) {
            layer->OnUpdate(dt);
         }

         {
            OPTICK_EVENT("OnImGuiRender");
            PROFILE_CPU("OnImGuiRender");

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

         {
            OPTICK_EVENT("ImGui Render");
            GPU_MARKER("ImGui Render");

            vec4 clearColor = {0.45f, 0.55f, 0.60f, 1.00f};
            cmd.ClearRenderTarget(sDevice->GetBackBuffer(), clearColor);

            cmd.SetRenderTargets(&sDevice->GetBackBuffer());
            imguiLayer->Render();
         }

         {
            OPTICK_EVENT("Present");
            sDevice->Present();
         }

         OPTICK_EVENT("Window Update");
         sWindow->Update();
      }

      for (auto* layer : layerStack) {
         layer->OnDetach();
      }

      imguiLayer = nullptr;
   }

}
