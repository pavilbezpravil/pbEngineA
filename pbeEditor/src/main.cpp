#include "EditorLayer.h"
#include "app/Application.h"


class ImGuiDemoWindow : public EditorWindow {
public:
   using EditorWindow::EditorWindow;

   void OnImGuiRender() override {
      if (show)
         ImGui::ShowDemoWindow(&show);
   }

   bool show = true;
};

class TestWindow : public EditorWindow {
public:
   using EditorWindow::EditorWindow;

   void OnImGuiRender() override {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

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

Application* CreateApplication() {
   Application* app = new Application();
   EditorLayer* editor = new EditorLayer();
   editor->AddEditorWindow(new ImGuiDemoWindow("ImGuiDemoWindow"));
   editor->AddEditorWindow(new TestWindow("TestWindow"));
   app->PushLayer(editor);
   return app;
}

