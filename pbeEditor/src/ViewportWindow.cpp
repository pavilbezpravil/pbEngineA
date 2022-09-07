#include "pch.h"
#include "ViewportWindow.h"

#include "EditorWindow.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "app/Input.h"
#include "rend/Renderer.h"
#include "rend/RendRes.h"


namespace pbe {

   ViewportWindow::ViewportWindow(std::string_view name): EditorWindow(name) {
      renderer.reset(new Renderer());
      renderer->Init();

      mat4 rotation = glm::rotate(mat4(1), glm::radians(cameraAngle.y), vec3_Right);
      rotation *= glm::rotate(mat4(1), glm::radians(cameraAngle.x), vec3_Up);

      float3 direction = vec4(0, 0, 1, 1) * rotation;

      camera.view = glm::lookAt(camera.position, camera.position + direction, vec3_Y);
   }

   void ViewportWindow::OnImGuiRender() {
      ImGui::Begin(name.c_str(), &show);

      enableInput = ImGui::IsWindowHovered();

      if (customHeadFunc) {
         customHeadFunc();
      }

      static RenderConfing cfg;

      ImGui::Checkbox("Transparency", &cfg.transparency);
      ImGui::SameLine();
      ImGui::Checkbox("Transparency Sorting", &cfg.transparencySorting);
      ImGui::SameLine();

      ImGui::Checkbox("Opaque Sorting", &cfg.opaqueSorting);
      ImGui::SameLine();

      ImGui::Checkbox("Decals", &cfg.decals);
      ImGui::SameLine();

      ImGui::Checkbox("Shadow", &cfg.useShadowPass);
      ImGui::SameLine();

      ImGui::Checkbox("Use ZPass", &cfg.useZPass);
      ImGui::SameLine();
      ImGui::Checkbox("SSAO", &cfg.ssao);
      ImGui::SameLine();
      ImGui::Checkbox("Use InstancedDraw", &cfg.useInstancedDraw);

      static bool textureViewWindow = false;
      ImGui::Checkbox("Texture View Window", &textureViewWindow);
      ImGui::SameLine();

      ImGui::Checkbox("Super Sampling", &cfg.superSampling);

      renderer->cfg = cfg;

      static int item_current = 0;
      const char* items[] = { "ColorLDR", "ColorHDR", "Depth", "LinearDepth", "Normal", "Position", "SSAO", "ShadowMap"};

      ImGui::SetNextItemWidth(80);
      ImGui::Combo("Scene RTs", &item_current, items, IM_ARRAYSIZE(items));

      CommandList cmd{ sDevice->g_pd3dDeviceContext };

      auto imSize = ImGui::GetContentRegionAvail();
      int2 size = { imSize.x, imSize.y };
      if (cfg.superSampling) {
         size *= 2;
      }

      if (size.x > 1 && size.y > 1) {
         if (!cameraContext.colorHDR || cameraContext.colorHDR->GetDesc().size != size) {
            Texture2D::Desc texDesc;
            texDesc.size = size;

            texDesc.name = "scene colorHDR";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            // texDesc.format = DXGI_FORMAT_R11G11B10_FLOAT; // my laptop doesnot support this format as UAV
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texDesc.bindFlags |= D3D11_BIND_UNORDERED_ACCESS; // todo:
            cameraContext.colorHDR = Texture2D::Create(texDesc);

            texDesc.name = "scene colorLDR";
            // texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            // texDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM; // todo: test srgb
            cameraContext.colorLDR = Texture2D::Create(texDesc);

            texDesc.name = "scene depth";
            // texDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            texDesc.format = DXGI_FORMAT_R24G8_TYPELESS;
            texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.depth = Texture2D::Create(texDesc);

            texDesc.name = "scene linear depth";
            texDesc.format = DXGI_FORMAT_R16_FLOAT;
            texDesc.mips = 0;
            texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            cameraContext.linearDepth = Texture2D::Create(texDesc);

            texDesc.mips = 1;

            // texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
            // texDesc.name = "scene depth copy";
            // cameraContext.depthCopy = Texture2D::Create(texDesc);

            texDesc.name = "scene normal";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_SNORM;
            texDesc.bindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.normal = Texture2D::Create(texDesc);

            texDesc.name = "scene position";
            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.position = Texture2D::Create(texDesc);

            texDesc.name = "scene ssao";
            texDesc.format = DXGI_FORMAT_R16_UNORM;
            texDesc.bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            cameraContext.ssao = Texture2D::Create(texDesc);

            if (!cameraContext.shadowMap) {
               Texture2D::Desc texDesc;
               texDesc.name = "shadow map";
               // texDesc.format = DXGI_FORMAT_D16_UNORM;
               texDesc.format = DXGI_FORMAT_R16_TYPELESS;
               texDesc.size = { 1024, 1024 };
               texDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
               cameraContext.shadowMap = Texture2D::Create(texDesc);
            }

            camera.zNear = 0.1f;
            camera.zFar = 1000.f;
            camera.projection = glm::perspectiveFov(90.f / (180) * PI, (float)texDesc.size.x, (float)texDesc.size.y, camera.zNear, camera.zFar);
         }

         cmd.SetCommonSamplers();
         if (scene) {
            renderer->RenderScene(cmd, *scene, camera, cameraContext);
         }
         cmd.pContext->ClearState(); // todo:

         auto gizmoCursorPos = ImGui::GetCursorScreenPos();

         Texture2D* sceneRTs[] = { cameraContext.colorLDR, cameraContext.colorHDR, cameraContext.depth, cameraContext.linearDepth, cameraContext.normal, cameraContext.position, cameraContext.ssao, cameraContext.shadowMap };
         ImGui::Image(sceneRTs[item_current]->srv.Get(), imSize);

         Gizmo(imSize, gizmoCursorPos);
      }

      ImGui::End();

      if (textureViewWindow && cameraContext.linearDepth) {
         ImGui::Begin("Texture View");

         auto texture = cameraContext.linearDepth;

         const auto& desc = texture->GetDesc();
         ImGui::Text("%s (%dx%d, %d)", desc.name.c_str(), desc.size.x, desc.size.y, desc.mips);

         static int iMip = 0;
         ImGui::SliderInt("mip", &iMip, 0, texture->GetDesc().mips - 1);
         ImGui::SameLine();

         int d = 1 << iMip;
         ImGui::Text("mip size (%dx%d)", desc.size.x / d, desc.size.y / d);

         auto imSize = ImGui::GetContentRegionAvail();
         auto srv = texture->GetMipSrv(iMip);

         // todo: hack. lambda with capture cant be passed as function pointer
         using dx11ContextType = decltype(cmd.pContext);
         static dx11ContextType sCtx = cmd.pContext;
         auto setPointSampler = [](const ImDrawList* cmd_list, const ImDrawCmd* pcmd) {
            sCtx->PSSetSamplers(0, 1, &rendres::samplerStateWrapPoint);
         };
         // cmd.SetCommonSamplers(); // todo:

         ImGui::GetWindowDrawList()->AddCallback(setPointSampler, nullptr);
         ImGui::Image(srv, imSize);
         ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

         ImGui::End();
      }
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

      // todo:
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
