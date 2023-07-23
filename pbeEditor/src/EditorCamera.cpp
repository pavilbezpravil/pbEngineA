#include "pch.h"
#include "EditorCamera.h"

#include "app/Input.h"

namespace pbe {

   void EditorCamera::Update(float dt) {
      if (Input::IsKeyPressing(VK_RBUTTON)) {
         float cameraMouseSpeed = 0.2f;
         cameraAngle += vec2(Input::GetMouseDelta()) * cameraMouseSpeed * vec2(-1, -1);
         cameraAngle.y = glm::clamp(cameraAngle.y, -85.f, 85.f);

         // todo: update use prev view matrix
         vec3 cameraInput{};
         if (Input::IsKeyPressing('A')) {
            cameraInput.x = -1;
         }
         if (Input::IsKeyPressing('D')) {
            cameraInput.x = 1;
         }
         if (Input::IsKeyPressing('Q')) {
            cameraInput.y = -1;
         }
         if (Input::IsKeyPressing('E')) {
            cameraInput.y = 1;
         }
         if (Input::IsKeyPressing('W')) {
            cameraInput.z = 1;
         }
         if (Input::IsKeyPressing('S')) {
            cameraInput.z = -1;
         }

         if (cameraInput != vec3{}) {
            cameraInput = glm::normalize(cameraInput);

            vec3 right = Right();
            vec3 up = Up();
            vec3 forward = Forward();

            vec3 cameraOffset = up * cameraInput.y + forward * cameraInput.z + right * cameraInput.x;

            float cameraSpeed = 5;
            if (Input::IsKeyPressing(VK_SHIFT)) {
               cameraSpeed *= 5;
            }

            position += cameraOffset * cameraSpeed * dt;
         }
      }

      UpdateView();
   }

   void EditorCamera::UpdateView() {
      mat4 rotation = glm::rotate(mat4(1), glm::radians(cameraAngle.y), vec3_Right);
      rotation *= glm::rotate(mat4(1), glm::radians(cameraAngle.x), vec3_Up);

      float3 direction = vec4(0, 0, 1, 1) * rotation;

      RenderCamera::UpdateView(direction);
   }
}
