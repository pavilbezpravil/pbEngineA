#include "pch.h"
#include "app/Application.h"
#include "fs/FileSystem.h"
#include "typer/Typer.h"
#include "scene/Scene.h"
#include "EditorLayer.h"
#include "core/Log.h"
#include "rend/Renderer.h"
#include "rend/Texture2D.h"

namespace pbe {

   class ImGuiDemoWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::ShowDemoWindow(&show);
      }
   };

   class TestWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         static float f = 0.0f;
         static int counter = 0;

         ImGui::Begin("Hello, world!", &show); // Create a window called "Hello, world!" and append into it.

         ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
         // ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state

         ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
         // ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

         if (ImGui::Button("Button"))
            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
         ImGui::SameLine();
         ImGui::Text("counter = %d", counter);

         ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
         ImGui::End();
      }
   };


   class ContentBrowserWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         // ImGui::Text("cwd %ls", fs::current_path().c_str());
         // ImGui::Text("tmp dir %ls", fs::temp_directory_path().c_str());

         ImGui::Text("current dir %s", fs::relative(currentPath, "./").string().c_str());

         if (ImGui::Button("<-")) {
            currentPath = currentPath.parent_path();
         }

         ImGui::Columns(5, 0, false);

         // for (auto& file : fs::recursive_directory_iterator(".")) {
         for (auto& file : fs::directory_iterator(currentPath)) {
            const auto& path = file.path();

            float size = 64;
            bool clicked = ImGui::Button(path.filename().string().c_str(), { size, size });
            ImGui::Text(path.filename().string().c_str());

            if (file.is_directory()) {
               if (clicked) {
                  currentPath = file.path();
               }
            }
            else {
               // ImGui::Text("%ls", file.path().filename().c_str());
            }

            ImGui::NextColumn();
         }

         ImGui::Columns(1);

         ImGui::End();
      }

   private:
      fs::path currentPath = fs::current_path();
   };

   class TyperWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         Typer::Get().ImGui();

         ImGui::End();
      }
   };

   class EditorApplication : public Application {
   public:
      void OnInit() override {
         Application::OnInit();

         EditorLayer* editor = new EditorLayer();
         editor->AddEditorWindow(new ImGuiDemoWindow("ImGuiDemoWindow"));
         editor->AddEditorWindow(new TestWindow("TestWindow"));
         editor->AddEditorWindow(new ContentBrowserWindow("ContentBrowser"), false);
         editor->AddEditorWindow(new TyperWindow("TyperWindow"), true);

         PushLayer(editor);
      }
   };

   Application* CreateApplication() {
      EditorApplication* app = new EditorApplication();
      return app;
   }

}
