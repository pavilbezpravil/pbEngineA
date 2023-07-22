#include "pch.h"
#include "app/Application.h"
#include "fs/FileSystem.h"
#include "typer/Typer.h"
#include "scene/Scene.h"
#include "EditorLayer.h"
#include "app/EntryPoint.h"
#include "core/Log.h"
#include "gui/Gui.h"
#include "rend/Renderer.h"

namespace pbe {

   class TestWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         static float f = 0.0f;
         static int counter = 0;

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
      }
   };


   class ContentBrowserWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         // ImGui::Text("cwd %ls", fs::current_path().c_str());
         // ImGui::Text("tmp dir %ls", fs::temp_directory_path().c_str());

         ImGui::Text("current dir %s", fs::relative(currentPath, "./").string().c_str());

         if (ImGui::Button("<-")) {
            currentPath = currentPath.parent_path();
         }

         int columns = (int)std::ceil(ImGui::GetContentRegionAvail().x / 128);
         ImGui::Columns(columns, 0, false);

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
            } else {
               // ImGui::Text("%ls", file.path().filename().c_str());
            }
            ImGui::NextColumn();
         }

         ImGui::Columns(1);

         if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create file")) {
               WriteStringToFile("hello", currentPath.append("test.txt").string().c_str());
            }
            if (ImGui::MenuItem("Open folder explorer")) {
               // write code to open file explorer in windows
               // write bubble sort algorithm
               ShellExecuteA(NULL, "open", currentPath.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
            }
            ImGui::EndPopup();
         }
      }

      void WriteStringToFile(std::string_view str, std::string_view filename) {
         std::ofstream file(filename.data());
         file << str;
         file.close();
      }
   private:
      // fs::path currentPath = fs::current_path();
      fs::path currentPath = "../../assets";
   };

   class TyperWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         Typer::Get().ImGui();
      }
   };

   class EditorApplication : public Application {
   public:
      void OnInit() override {
         Application::OnInit();

         EditorLayer* editor = new EditorLayer();
         editor->AddEditorWindow(new TestWindow("TestWindow"));
         editor->AddEditorWindow(new ContentBrowserWindow("ContentBrowser"), false);
         editor->AddEditorWindow(new TyperWindow("TyperWindow"), true);

         PushLayer(editor);
      }
   };

}

int main(int nArgs, char** args) {
   pbe::EditorApplication* app = new pbe::EditorApplication();
   return pbeMain(app, nArgs, args);
}
