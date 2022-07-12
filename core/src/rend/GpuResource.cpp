#include "GpuResource.h"

#include <d3d11.h>
#include "Common.h"

namespace pbe {

   GPUResource::GPUResource(ID3D11Resource* pResource) : pResource(pResource) {

   }

   GPUResource::~GPUResource() {
      SAFE_RELEASE(pResource);
   }

   void GPUResource::SetDbgName(std::string_view dbgName) {
      ::pbe::SetDbgName(pResource, dbgName);
   }

}
