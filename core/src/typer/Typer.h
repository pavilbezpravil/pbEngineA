#pragma once

#include "core/Core.h"
#include "core/Type.h"


namespace pbe {

   struct Serializer;
   struct Deserializer;

   class Entity;
   class Scene;

   struct TypeField {
      std::string name;
      TypeID typeID;
      size_t offset;
      std::function<bool(const char*, byte*)> uiFunc;
   };

   struct TypeInfo {
      std::string name;
      TypeID typeID;
      int typeSizeOf;
      bool hasEntityRef = false;

      std::vector<TypeField> fields;

      std::function<bool(const char*, byte*)> ui;
      std::function<void(Serializer&, const byte*)> serialize;
      std::function<bool(const Deserializer&, byte*)> deserialize;

      bool IsSimpleType() const { return fields.empty(); }
   };

   struct ComponentInfo {
      TypeID typeID;

      std::function<void* (Entity&, const void*)> copyCtor;
      std::function<void* (Entity&, const void*)> moveCtor;

      std::function<bool (const Entity&)> has;
      std::function<void* (Entity&)> add;
      std::function<void (Entity&)> remove;
      std::function<void* (Entity&)> get; // todo: remove?

      std::function<void* (Entity&)> getOrAdd;
      std::function<void* (Entity&)> tryGet;
      std::function<const void* (const Entity&)> tryGetConst; // todo: remove?

      std::function<void(void*, const void*)> duplicate; // todo: remove

      std::function<void(void*)> onChanged; // todo: remove?

      // todo:
      // ComponentInfo() = default;
      // ComponentInfo(ComponentInfo&&) = default;
      // ONLY_MOVE(ComponentInfo);
   };

   struct ScriptInfo {
      using ApplyFunc = std::function<void(class Script&)>;

      TypeID typeID;
      std::function<void(Scene&, const ApplyFunc&)> sceneApplyFunc;
   };

   class CORE_API Typer {
   public:
      Typer();

      static Typer& Get();

      void ImGui();
      void ImGuiTypeInfo(const TypeInfo& ti);

      void RegisterType(TypeID typeID, TypeInfo&& ti);
      void UnregisterType(TypeID typeID);

      void RegisterComponent(ComponentInfo&& ci);
      void UnregisterComponent(TypeID typeID);

      void RegisterScript(ScriptInfo&& si);
      void UnregisterScript(TypeID typeID);

      template<typename T>
      const TypeInfo& GetTypeInfo() const {
         return GetTypeInfo(GetTypeID<T>());
      }
      const TypeInfo& GetTypeInfo(TypeID typeID) const;

      template<typename T>
      TypeInfo& GetTypeInfo() {
         return GetTypeInfo(GetTypeID<T>());
      }
      TypeInfo& GetTypeInfo(TypeID typeID);

      bool UI(std::string_view name, TypeID typeID, byte* value) const;
      void Serialize(Serializer& ser, std::string_view name, TypeID typeID, const byte* value) const;
      bool Deserialize(const Deserializer& deser, std::string_view name, TypeID typeID, byte* value) const;

      void Finalize();

      std::unordered_map<TypeID, TypeInfo> types;

      std::vector<ComponentInfo> components;
      std::vector<ScriptInfo> scripts;

   private:
      void ProcessType(TypeInfo& ti, std::unordered_set<TypeID>& processedTypeIDs);
   };

}
