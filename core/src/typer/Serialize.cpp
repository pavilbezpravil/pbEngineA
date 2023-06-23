#include "pch.h"
#include "Serialize.h"

#include "Typer.h"


namespace pbe {

   void Serializer::Ser(std::string_view name, TypeID typeID, const byte* value)
   {
      Typer::Get().SerializeImpl(out, name, typeID, value);
   }

   bool Serializer::SaveToFile(string_view filename)
   {
      std::ofstream fout{ filename.data() };
      fout << out.c_str();

      return true; // todo:
   }

   Deserializer Deserializer::FromFile(string_view filename)
   {
      std::ifstream fin{ filename.data() };
      return Deserializer{ YAML::Load(fin) };
   }

   bool Deserializer::Deser(std::string_view name, TypeID typeID, byte* value)
   {
      return Typer::Get().DeserializeImpl(node, name, typeID, value);
   }

   Deserializer::Deserializer(YAML::Node node) : node(std::move(node))
   {
   }
}
