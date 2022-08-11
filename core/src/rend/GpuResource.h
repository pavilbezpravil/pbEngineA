#pragma once

#include "core/Core.h"
#include "core/Common.h"
#include "Common.h"
#include "core/Ref.h"

struct ID3D11Resource;

namespace pbe {

   class CORE_API GPUResource : public RefCounted {
      NON_COPYABLE(GPUResource);
   public:
      GPUResource(ID3D11Resource* pResource);

      virtual ~GPUResource();

      void SetDbgName(std::string_view dbgName);

      ComPtr<ID3D11Resource> pResource;
      ComPtr<ID3D11ShaderResourceView> srv;
      ComPtr<ID3D11UnorderedAccessView> uav;
   };


}
