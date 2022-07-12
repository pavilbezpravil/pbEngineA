#include "UUID.h"
#include "typer/Typer.h"

#include <random>

namespace pbe {

   static std::random_device s_RandomDevice;
   static std::mt19937_64 sEng(s_RandomDevice());
   static std::uniform_int_distribution<uint64_t> sDistribution;

   // todo:: add custom ser deser
   TYPER_BEGIN(UUID)
   // ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(Type*)value; }; \
   // ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(Type*)value = node[name].as<Type>(); };
   TYPER_END(UUID)
   

   UUID::UUID() : uuid(sDistribution(sEng)) {
   }

   bool UUID::Valid() const {
      return uuid != (uint64)UUID_INVALID;
   }

}
