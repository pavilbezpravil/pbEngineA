#include "pch.h"
#include "ViewportWindow.h"

#include "EditorWindow.h"
#include "imgui.h"
#include "app/Input.h"
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

      windowFocused = ImGui::IsWindowFocused();

      if (ImGui::Button("Play")) {
         INFO("Play pressed!");
      }

      ImGui::SliderFloat("cameraAngle", &renderer->angle, -180, 180);

      ImGui::Text("ColorPass: %.3f ms", renderer->timer.GetTimeMs());

      auto size = ImGui::GetContentRegionAvail();

      CommandList cmd{ sDevice->g_pd3dDeviceContext };
      renderer->RenderScene(*colorTexture, *depthTexture, cmd, *scene);

      ImGui::Image(colorTexture->srv, size);

      ImGui::End();
   }

   void ViewportWindow::OnUpdate(float dt) {
      if (!windowFocused) {
         return;
      }

      vec3 cameraInput{};

      if (Input::IsKeyPressed('A')) {
         cameraInput.x = -1;
      }
      if (Input::IsKeyPressed('D')) {
         cameraInput.x = 1;
      }
      if (Input::IsKeyPressed('Q')) {
         cameraInput.y = -1;
      }
      if (Input::IsKeyPressed('E')) {
         cameraInput.y = 1;
      }
      if (Input::IsKeyPressed('W')) {
         cameraInput.z = 1;
      }
      if (Input::IsKeyPressed('S')) {
         cameraInput.z = -1;
      }

      if (cameraInput != vec3{}) {
         cameraInput = glm::normalize(cameraInput);

         vec3 right = vec3_X;
         vec3 up = vec3_Y;
         vec3 forward = vec3_Z;

         vec3 cameraOffset = up * cameraInput.y + forward * cameraInput.z + right * cameraInput.x;

         renderer->cameraPos += cameraOffset * dt * 3.f;
      }

      if (Input::IsKeyPressed(VK_RBUTTON)) {
         renderer->angle += float(-Input::GetMouseDelta().x) * dt * 3.f;
      }

      // INFO("Left {} Right {}", Input::IsKeyPressed(VK_LBUTTON), Input::IsKeyPressed(VK_RBUTTON));
   }
}
