#include "pch.h"
#include "ViewportWindow.h"
#include "EditorWindow.h"

#include "imgui.h"
#include "app/Event.h"
#include "gui/Gui.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "rend/Renderer.h"


namespace pbe {

   ViewportWindow::ViewportWindow(std::string_view name): EditorWindow(name) {
      Texture2D::Desc sceneTexDesc;
      sceneTexDesc.format = DXGI_FORMAT_R16G16B16A16_UNORM;
      sceneTexDesc.bindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
      sceneTexDesc.size = { 640, 480 };

      sceneTexture = Texture2D::Create(sceneTexDesc);
      sceneTexture->SetDbgName("scene color");

      renderer.reset(new Renderer());
      renderer->Init();
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      if (ImGui::Button("Play")) {
         INFO("Play pressed!");
      }

      ImGui::InputFloat3("cameraPos", &renderer->cameraPos.x);
      ImGui::SliderFloat("cameraAngle", &renderer->angle, -180, 180);

      auto size = ImGui::GetContentRegionAvail();

      CommandList cmd{ sDevice->g_pd3dDeviceContext };
      renderer->RenderScene(*sceneTexture, cmd, *scene);

      ImGui::Image(sceneTexture->srv, size);

      ImGui::End();
   }
}
