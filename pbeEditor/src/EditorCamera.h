#pragma once

#include "app/Input.h"
#include "math/Types.h"

namespace pbe {

   struct EditorCamera {
      vec3 position{};
      vec2 cameraAngle{};

      mat4 view{};
      mat4 projection{};

      void UpdateProj(int2 size) {
         projection = glm::perspectiveFov(90.f / 180 * PI, (float)size.x, (float)size.y, zNear, zFar);
      }

      mat4 prevViewProjection{};

      float zNear = 0.1f;
      float zFar = 1000.f;

      vec3 Right() const {
         return vec3{ view[0][0], view[1][0] , view[2][0] };
      }

      vec3 Up() const {
         return vec3{ view[0][1], view[1][1] , view[2][1] };
      }

      vec3 Forward() const {
         return vec3{ view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }

      void Update(float dt) {
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

      void UpdateView() {
         mat4 rotation = glm::rotate(mat4(1), glm::radians(cameraAngle.y), vec3_Right);
         rotation *= glm::rotate(mat4(1), glm::radians(cameraAngle.x), vec3_Up);

         float3 direction = vec4(0, 0, 1, 1) * rotation;

         view = glm::lookAt(position, position + direction, vec3_Y);
      }

      void NextFrame() {
         prevViewProjection = GetViewProjection();
      }

   };

}
