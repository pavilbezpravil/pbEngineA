#include "pch.h"
#include "Application.h"

#include "Input.h"
#include "core/Common.h"
#include "core/Log.h"
#include "core/Profiler.h"
#include "rend/Device.h"
#include "rend/CommandList.h"
#include "Window.h"
#include "core/CVar.h"
#include "core/Thread.h"
#include "gui/ImGuiLayer.h"
#include "physics/Phys.h"
#include "rend/RendRes.h"
#include "rend/Shader.h"
#include "typer/Typer.h"


namespace pbe {

   Application* sApplication = nullptr;

   void Application::OnInit() {
      Typer::Get().Finalize();

      Log::Init();

      new Window();

      new Device();
      if (!sDevice->created) {
         return;
      }

      InitPhysics();

      rendres::Init();

      Profiler::Init();

      sWindow->eventCallback = [&](Event& event) { OnEvent(event); };

      ImGui::CreateContext();

      imguiLayer = new ImGuiLayer();
      PushOverlay(imguiLayer);

      INFO("App init success");
      running = true;
   }

   void Application::OnTerm() {
      rendres::Term();
      TermGpuPrograms();

      TermPhysics();

      layerStack.Clear();

      ImGui::DestroyContext();

      Profiler::Term();

      SAFE_DELETE(sDevice);
      SAFE_DELETE(sWindow);
   }

   void Application::OnEvent(Event& event) {
      Input::OnEvent(event);

      if (event.GetEvent<AppQuitEvent>()) {
         INFO("App quit event");
         running = false;
      }
      if (event.GetEvent<AppLoseFocusEvent>()) {
         focused = false;
      }
      if (event.GetEvent<AppGetFocusEvent>()) {
         focused = true;
      }

      if (auto* windowResize = event.GetEvent<WindowResizeEvent>()) {
         sDevice->Resize(windowResize->size);
      }

      if (auto* key = event.GetEvent<KeyDownEvent>()) {
         if (Input::IsKeyPressing(VK_CONTROL) && key->keyCode == 'R') {
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

      float fpsTimer = 0;
      int frames = 0;

      // Main loop
      while (running) {
         OPTICK_FRAME("MainThread");

         if (!focused) {
            // OPTICK_EVENT("Sleep On Focused");
            // ThreadSleepMs(250);
         }

         {
            OPTICK_EVENT("Window Update");
            Input::ClearKeys();
            sWindow->Update();
            Input::NextFrame();

            // todo:
            if (!running) {
               break;
            }
         }

         float dt = frameTimer.ElapsedMs(true) / 1000.f;
         // INFO("DeltaTime: {} ms", dt * 1000.f);

         fpsTimer += dt;
         frames++;
         if (fpsTimer > 1) {
            // todo: show it editor
            float fps = (float)frames / fpsTimer;
            // INFO("fps: {}", fps);
            fpsTimer -= 1;
            frames = 0;
         }

         // debug handle
         if (dt > 1.f) {
            dt = 1.f / 60.f;
         }
         OPTICK_TAG("DeltaTime (ms)", dt * 1000.f);

         // todo: different interface
         sConfigVarsMng.NextFrame(); // todo: use before triggered in that frame
         Profiler::Get().NextFrame();
         ShadersSrcWatcherUpdate();

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
      }

      for (auto* layer : layerStack) {
         layer->OnDetach();
      }

      imguiLayer = nullptr;
   }

   const char* Application::GetBuildType() {
      #if defined(DEBUG)
         return "Debug";
      #elif defined(RELEASE)
         return "Release";
      #else
         #error Undefined configuration?
      #endif
   }

}
