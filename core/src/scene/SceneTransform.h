#pragma once

#include "Entity.h"
#include "core/UUID.h"
#include "math/Transform.h"
#include "math/Types.h"

namespace pbe {

   struct TransformChangedMarker {};

   struct CORE_API SceneTransformComponent {
      SceneTransformComponent() = default;
      SceneTransformComponent(Entity entity, Entity parent = {});

      Entity entity;
      Entity parent;
      std::vector<Entity> children;

      const Transform& Local() const;
      Transform& Local();
      Transform World() const;

      vec3 Position(Space space = Space::World) const;
      quat Rotation(Space space = Space::World) const;
      vec3 Scale(Space space = Space::World) const;

      SceneTransformComponent& SetTransform(const Transform& transform, Space space = Space::World);
      SceneTransformComponent& SetPosition(const vec3& pos, Space space = Space::World);
      SceneTransformComponent& SetRotation(const quat& rot, Space space = Space::World);
      SceneTransformComponent& SetScale(const vec3& s, Space space = Space::World);

      vec3 Right() const;
      vec3 Up() const;
      vec3 Forward() const;

      mat4 GetWorldMatrix() const;
      mat4 GetPrevMatrix() const;
      void SetMatrix(const mat4& transform);

      void UpdatePrevTransform(); // todo: call it when first create entity

      bool HasParent() const { return (bool)parent; }
      bool HasChilds() const { return !children.empty(); }

      SceneTransformComponent& AddChild(Entity child, int iChild = -1, bool keepLocalTransform = false);
      SceneTransformComponent& RemoveChild(int idx);
      SceneTransformComponent& RemoveAllChild(Entity theirNewParent = {});

      SceneTransformComponent& SetParent(Entity newParent = {}, int iChild = -1, bool keepLocalTransform = false);
      SceneTransformComponent& SetParentInternal(Entity newParent = {}, int iChild = -1, bool keepLocalTransform = false);
      int GetChildIdx() const;

      void Serialize(Serializer& ser) const;
      bool Deserialize(const Deserializer& deser);
      bool UI();

      auto begin() { return children.begin(); }
      auto end() { return children.end(); }

   private:
      Transform local{
         .position = vec3_Zero,
         .rotation = quat_Identity,
         .scale = vec3_One,
      };

      // todo:
      Transform prevWorld{
         .position = vec3_Zero,
         .rotation = quat_Identity,
         .scale = vec3_One,
      };
   };

}
