#pragma once

#include <string_view>

#include "core/Ref.h"
#include "core/Common.h"

struct ID3D11Resource;

namespace pbe {

   class GPUResource : public RefCounted {
      NON_COPYABLE(GPUResource);
   public:
      GPUResource(ID3D11Resource* pResource);

      virtual ~GPUResource();

      void SetDbgName(std::string_view dbgName);

      ID3D11Resource* pResource{};
   };


}
