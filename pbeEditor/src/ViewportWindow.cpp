#include "pch.h"
#include "ViewportWindow.h"

#include "EditorWindow.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "app/Input.h"
#include "rend/Renderer.h"

namespace pbe {

   ViewportWindow::ViewportWindow(std::string_view name): EditorWindow(name) {
      renderer.reset(new Renderer());
      renderer->Init();
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      enableInput = ImGui::IsWindowHovered();

      // if (ImGui::Button("Play")) {
      //    INFO("Play pressed!");
      // }

      static RenderConfing cfg;

      ImGui::Checkbox("Transparency", &cfg.transparency);
      ImGui::SameLine();
      ImGui::Checkbox("Transparency Sorting", &cfg.transparencySorting);
      ImGui::SameLine();

      ImGui::Checkbox("Opaque Sorting", &cfg.opaqueSorting);
      ImGui::SameLine();

      ImGui::Checkbox("Decals", &cfg.decals);
      ImGui::SameLine();

      ImGui::Checkbox("Use ZPass", &cfg.useZPass);
      ImGui::SameLine();
      ImGui::Checkbox("SSAO", &cfg.ssao);
      ImGui::SameLine();
      ImGui::Checkbox("Use InstancedDraw", &cfg.useInstancedDraw);

      renderer->cfg = cfg;

      static int item_current = 0;
      const char* items[] = { "Color", "Depth", "Normal", "Position", "SSAO" };

      ImGui::SetNextItemWidth(80);
      ImGui::Combo("Scene RTs", &item_current, items, IM_ARRAYSIZE(items));

      auto imSize = ImGui::GetContentRegionAvail();
      int2 size = { imSize.x, imSize.y };
      if (size.x > 1 && size.y > 1) {
         if (!cameraContext.colorHDR || cameraContext.colorHDR->GetDesc().size != size) {
            Texture2D::Desc texDesc;
            texDesc.format = DXGI_FORMAT_R16G16B16A16_UNORM;
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texDesc.size = size;

            cameraContext.colorHDR = Texture2D::Create(texDesc);
            cameraContext.colorHDR->SetDbgName("scene colorHDR");

            // texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            texDesc.format = DXGI_FORMAT_R24G8_TYPELESS;
            texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

            cameraContext.depth = Texture2D::Create(texDesc);
            cameraContext.depth->SetDbgName("scene depth");

            texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
            cameraContext.depthCopy = Texture2D::Create(texDesc);
            cameraContext.depthCopy->SetDbgName("scene depth copy");

            texDesc.format = DXGI_FORMAT_R16G16B16A16_SNORM;
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.normal = Texture2D::Create(texDesc);
            cameraContext.normal->SetDbgName("scene normal");

            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.position = Texture2D::Create(texDesc);
            cameraContext.position->SetDbgName("scene position");

            texDesc.format = DXGI_FORMAT_R16_UNORM;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.ssao = Texture2D::Create(texDesc);
            cameraContext.ssao->SetDbgName("scene ssao");

            camera.zNear = 0.1f;
            camera.zFar = 200.f;
            camera.projection = glm::perspectiveFov(90.f / (180) * pi, (float)texDesc.size.x, (float)texDesc.size.y, camera.zNear, camera.zFar);
         }

         CommandList cmd{ sDevice->g_pd3dDeviceContext };
         if (scene) {
            renderer->RenderScene(cmd, *scene, camera, cameraContext);
         }
         cmd.pContext->ClearState(); // todo:

         auto gizmoCursorPos = ImGui::GetCursorScreenPos();

         Texture2D* sceneRTs[] = { cameraContext.colorHDR, cameraContext.depth, cameraContext.normal, cameraContext.position, cameraContext.ssao };
         ImGui::Image(sceneRTs[item_current]->srv.Get(), imSize);

         Gizmo(imSize, gizmoCursorPos);
      }

      ImGui::End();
   }

   void ViewportWindow::OnUpdate(float dt) {
      if (!enableInput) {
         return;
      }

      if (Input::IsKeyPressed(VK_RBUTTON)) {
         ImGui::SetWindowFocus(name.data());

         float cameraMouseSpeed = 10;
         cameraAngle += vec2(Input::GetMouseDelta()) * dt * cameraMouseSpeed * vec2(-1, -1);
         cameraAngle.y = glm::clamp(cameraAngle.y, -85.f, 85.f);

         // todo: update use prev view matrix
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
            vec3 forward = camera.Forward();

            vec3 cameraOffset = up * cameraInput.y + forward * cameraInput.z + right * cameraInput.x;

            float cameraSpeed = 5;
            if (Input::IsKeyPressed(VK_SHIFT)) {
               cameraSpeed *= 5;
            }

            camera.position += cameraOffset * cameraSpeed * dt;
         }
      } else {
         if (Input::IsKeyPressed('W')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::TRANSLATE;
         }
         if (Input::IsKeyPressed('R')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::ROTATE;
         }
         if (Input::IsKeyPressed('S')) {
            gizmoCfg.operation = ImGuizmo::OPERATION::SCALE;
         }

         // todo: IsKeyDown
         // if (Input::IsKeyPressed('Q')) {
         //    gizmoCfg.space = 1 - gizmoCfg.space;
         // }
         if (Input::IsKeyPressed('Q')) {
            gizmoCfg.space = 0;
         }
         if (Input::IsKeyPressed('A')) {
            gizmoCfg.space = 1;
         }

         if (Input::IsKeyPressed('F') && selection->FirstSelected()) {
            auto selectedEntity = selection->FirstSelected();
            camera.position = selectedEntity.Get<SceneTransformComponent>().position - camera.Forward() * 3.f;
         }
      }

      mat4 rotation = glm::rotate(mat4(1), glm::radians(cameraAngle.y), vec3_Right);
      rotation *= glm::rotate(mat4(1), glm::radians(cameraAngle.x), vec3_Up);

      float3 direction = vec4(0, 0, 1, 1) * rotation;

      camera.view = glm::lookAt(camera.position, camera.position + direction, vec3_Y);

      // INFO("Left {} Right {}", Input::IsKeyPressed(VK_LBUTTON), Input::IsKeyPressed(VK_RBUTTON));
   }

   void ViewportWindow::Gizmo(const ImVec2& contentRegion, const ImVec2& cursorPos) {
      auto selectedEntity = selection->FirstSelected();
      if (!selectedEntity.Valid()) {
         return;
      }

      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
      ImGuizmo::SetRect(cursorPos.x, cursorPos.y, contentRegion.x, contentRegion.y);

      bool snap = Input::IsKeyPressed(VK_CONTROL);

      mat4 entityTransform = selectedEntity.Get<SceneTransformComponent>().GetMatrix();

      float snapValue = 1;
      float snapValues[3] = { snapValue, snapValue, snapValue };

      ImGuizmo::Manipulate(glm::value_ptr(camera.view),
         glm::value_ptr(camera.projection),
         (ImGuizmo::OPERATION)gizmoCfg.operation,
         (ImGuizmo::MODE)gizmoCfg.space,
         glm::value_ptr(entityTransform),
         nullptr,
         snap ? snapValues : nullptr);

      if (ImGuizmo::IsUsing()) {
         selectedEntity.Get<SceneTransformComponent>().SetMatrix(entityTransform);
      }
   }
}
