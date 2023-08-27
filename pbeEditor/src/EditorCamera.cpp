#include "pch.h"
#include "EditorCamera.h"

#include "app/Input.h"

namespace pbe {

   void EditorCamera::Update(float dt) {
      if (Input::IsKeyPressing(KeyCode::RightButton)) {
         float cameraMouseSpeed = 0.2f / 50;
         cameraAngle += vec2(Input::GetMouseDelta()) * cameraMouseSpeed * vec2(-1, -1);
         cameraAngle.y = glm::clamp(cameraAngle.y, -PIHalf * 0.95f, PIHalf * 0.95f);

         // todo: update use prev view matrix
         vec3 cameraInput{};
         if (Input::IsKeyPressing(KeyCode::A)) {
            cameraInput.x = -1;
         }
         if (Input::IsKeyPressing(KeyCode::D)) {
            cameraInput.x = 1;
         }
         if (Input::IsKeyPressing(KeyCode::Q)) {
            cameraInput.y = -1;
         }
         if (Input::IsKeyPressing(KeyCode::E)) {
            cameraInput.y = 1;
         }
         if (Input::IsKeyPressing(KeyCode::W)) {
            cameraInput.z = 1;
         }
         if (Input::IsKeyPressing(KeyCode::S)) {
            cameraInput.z = -1;
         }

         if (cameraInput != vec3{}) {
            cameraInput = glm::normalize(cameraInput);

            vec3 right = Right();
            vec3 up = Up();
            vec3 forward = Forward();

            vec3 cameraOffset = up * cameraInput.y + forward * cameraInput.z + right * cameraInput.x;

            float cameraSpeed = 5;
            if (Input::IsKeyPressing(KeyCode::Shift)) {
               cameraSpeed *= 5;
            }

            position += cameraOffset * cameraSpeed * dt;
         }
      }

      UpdateView();
   }

   void EditorCamera::UpdateView() {
      mat4 rotation = glm::rotate(mat4(1), cameraAngle.y, vec3_Right);
      rotation *= glm::rotate(mat4(1), cameraAngle.x, vec3_Up);

      float3 direction = vec4(0, 0, 1, 1) * rotation;

      UpdateViewByDirection(direction);
   }

   void EditorCamera::SetViewDirection(const vec3& direction) {
      cameraAngle.x = atan2(direction.x, direction.z);
      cameraAngle.y = asin(direction.y);

      UpdateView();
   }


}
