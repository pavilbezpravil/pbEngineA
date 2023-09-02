#pragma once
#include "math/Types.h"

namespace pbe {

   class Texture2D;

   struct DenoiseCallDesc {
      // from REBLUR_DIFFUSE
      // INPUTS - IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST,
      // OPTIONAL INPUTS - IN_DIFF_CONFIDENCE
      // OUTPUTS - OUT_DIFF_RADIANCE_HITDIST

      // from REBLUR_DIFFUSE
      // INPUTS - IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_SPEC_RADIANCE_HITDIST,
      // OPTIONAL INPUTS - IN_SPEC_DIRECTION_PDF, IN_SPEC_CONFIDENCE
      // OUTPUTS - OUT_SPEC_RADIANCE_HITDIST

      uint2 textureSize{};

      mat4 mViewToClip{};
      mat4 mWorldToView{};

      mat4 mViewToClipPrev{};
      mat4 mWorldToViewPrev{};

      Texture2D* IN_MV{};
      Texture2D* IN_NORMAL_ROUGHNESS{};
      Texture2D* IN_VIEWZ{};
      Texture2D* IN_DIFF_RADIANCE_HITDIST{};
      Texture2D* IN_SPEC_RADIANCE_HITDIST{};

      Texture2D* OUT_DIFF_RADIANCE_HITDIST{};
      Texture2D* OUT_SPEC_RADIANCE_HITDIST{};
   };

   void NRDInit();
   void NRDDenoisersInit(uint2 renderResolution);
   void NRDDenoise(const DenoiseCallDesc& desc);
   void NRDTerm(); // call on resize too

   // todo:
   void NRDResize(uint2 renderResolution);

}
