#include "pch.h"
#include "GpuResource.h"
#include "Common.h"

namespace pbe {

   GPUResource::GPUResource(ID3D11Resource* pResource) : pResource(pResource) {

   }

   GPUResource::~GPUResource() {}

   void GPUResource::SetDbgName(std::string_view dbgName) {
      if (pResource) {
         ::pbe::SetDbgName(pResource.Get(), dbgName);
      }
   }

}
