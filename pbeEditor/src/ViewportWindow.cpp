#include "pch.h"
#include "ViewportWindow.h"

#include "EditorWindow.h"
#include "imgui.h"
#include "scene/Scene.h"
#include "rend/Renderer.h"

namespace pbe {

   ViewportWindow::ViewportWindow(std::string_view name): EditorWindow(name) {
      Texture2D::Desc texDesc;
      texDesc.format = DXGI_FORMAT_R16G16B16A16_UNORM;
      texDesc.bindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
      texDesc.size = { 640, 480 };

      colorTexture = Texture2D::Create(texDesc);
      colorTexture->SetDbgName("scene color");

      texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
      texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL;

      depthTexture = Texture2D::Create(texDesc);
      depthTexture->SetDbgName("scene depth");

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
      renderer->RenderScene(*colorTexture, *depthTexture, cmd, *scene);

      ImGui::Image(colorTexture->srv, size);

      ImGui::End();
   }
}
