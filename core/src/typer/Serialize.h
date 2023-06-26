#pragma once

#include "core/Core.h"
#include "core/Type.h"
#include "fs/FileSystem.h"

namespace YAML {
   class Node;
   class Emitter;
}

namespace pbe {

   struct CORE_API Serializer {
      template<typename T>
      void Ser(std::string_view name, const T& value) {
         const auto typeID = GetTypeID<T>();
         Ser(name, typeID, (const byte*)&value);
      }

      void Ser(std::string_view name, TypeID typeID, const byte* value);

      template<typename Key>
      void Key(const Key& key) {
         out << YAML::Key << key << YAML::Value;
      }

      template<typename Key, typename Value>
      void KeyValue(const Key& key, const Value& value) {
         out << YAML::Key << key << YAML::Value << value;
      }

      bool SaveToFile(string_view filename);

      YAML::Emitter out;
   };

   struct CORE_API Deserializer {
      static Deserializer FromFile(string_view filename);

      template<typename T>
      T Deser(std::string_view name) const{
         T value;
         Deser(name, value);
         return value;
      }

      template<typename T>
      bool Deser(std::string_view name, T& value) const {
         const auto typeID = GetTypeID<T>();
         return Deser(name, typeID, (byte*)&value);
      }

      bool Deser(std::string_view name, TypeID typeID, byte* value) const;

      template <typename T>
      T As() const {
         return node.as<T>();
      }

      explicit operator bool() const { return (bool)node; }
      bool operator!() const { return !node; }

      std::size_t Size() const { return node.size(); }

      template <typename Key>
      const Deserializer operator[](const Key& key) const {
         return node[key];
      }
      
      template <typename Key>
      Deserializer operator[](const Key& key) {
         return node[key];
      }

      YAML::Node node;

   private:
      Deserializer(YAML::Node node);
   };

   template<typename T>
   void Serialize(std::string_view filename, const T& value) {
      Serializer ser;
      ser.Ser("", value);
      ser.SaveToFile(filename);
   }

   template<typename T>
   bool Deserialize(std::string_view filename, T& value) {
      if (!fs::exists(filename)) {
         return false;
      }
      auto deser  = Deserializer::FromFile(filename);
      return deser.Deser("", value);
   }

   struct SerializerMap {
      SerializerMap(Serializer& ser) : ser(ser) {
         ser.out << YAML::BeginMap;
      }

      ~SerializerMap() {
         ser.out << YAML::EndMap;
      }

      Serializer& ser;
   };

#define SERIALIZER_MAP(ser) SerializerMap serializerMap{ser}

   struct SerializerSeq {
      SerializerSeq(Serializer& ser) : ser(ser) {
         ser.out << YAML::BeginSeq;
      }

      ~SerializerSeq() {
         ser.out << YAML::EndSeq;
      }

      Serializer& ser;
   };

#define SERIALIZER_SEQ(ser) SerializerSeq serializerSeq{ser}
}
