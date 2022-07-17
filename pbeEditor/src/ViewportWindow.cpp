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
      // texDesc.size *= 3;

      colorTexture = Texture2D::Create(texDesc);
      colorTexture->SetDbgName("scene color");

      texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
      texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL;

      depthTexture = Texture2D::Create(texDesc);
      depthTexture->SetDbgName("scene depth");

      renderer.reset(new Renderer());
      renderer->Init();

      camera.projection = glm::perspectiveFov(90.f / (180) * pi, (float)texDesc.size.x, (float)texDesc.size.y, 0.1f, 200.f);
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      windowFocused = ImGui::IsWindowFocused();

      if (ImGui::Button("Play")) {
         INFO("Play pressed!");
      }

      ImGui::Checkbox("Render Transparency", &renderer->renderTransparency);
      ImGui::SameLine();
      ImGui::Checkbox("Transparency Sorting", &renderer->transparencySorting);

      ImGui::Checkbox("Opaque Sorting", &renderer->opaqueSorting);
      ImGui::SameLine();
      ImGui::Checkbox("Use ZPass", &renderer->useZPass);

      ImGui::Checkbox("Use InstancedDraw", &renderer->useInstancedDraw);

      auto size = ImGui::GetContentRegionAvail();

      CommandList cmd{ sDevice->g_pd3dDeviceContext };
      renderer->RenderScene(*colorTexture, *depthTexture, cmd, *scene, camera);

      ImGui::Image(colorTexture->srv, size);

      ImGui::End();
   }

   void ViewportWindow::OnUpdate(float dt) {
      if (!windowFocused) {
         return;
      }

      if (Input::IsKeyPressed(VK_RBUTTON)) {
         float cameraMouseSpeed = 10;
         cameraAngle += vec2(Input::GetMouseDelta()) * dt * cameraMouseSpeed * vec2(-1, -1);

         cameraAngle.y = glm::clamp(cameraAngle.y, -85.f, 85.f);
      }

      mat4 rotation = glm::rotate(mat4(1), glm::radians(cameraAngle.y), vec3_Right);
      rotation *= glm::rotate(mat4(1), glm::radians(cameraAngle.x), vec3_Up);

      float3 direction = vec4(0, 0, 1, 1) * rotation;

      camera.view = glm::lookAt(camera.position, camera.position + direction, vec3_Y);

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

         mat4 viewTrans = glm::transpose(camera.view);

         vec3 right = viewTrans[0];
         vec3 up = viewTrans[1];
         // vec3 forward = viewTrans[2];
         vec3 forward = camera.Forward();

         vec3 cameraOffset = up * cameraInput.y + forward * cameraInput.z + right * cameraInput.x;

         float cameraSpeed = 5;
         if (Input::IsKeyPressed(VK_SHIFT)) {
            cameraSpeed *= 5;
         }

         camera.position += cameraOffset * cameraSpeed * dt;
      }

      // INFO("Left {} Right {}", Input::IsKeyPressed(VK_LBUTTON), Input::IsKeyPressed(VK_RBUTTON));
   }
}
