#pragma once
#include "math/Types.h"

namespace pbe {
   class CommandList;

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

      // from REBLUR_DIFFUSE_SPECULAR
      // INPUTS - IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST, IN_SPEC_RADIANCE_HITDIST,
      // OPTIONAL INPUTS - IN_DIFF_CONFIDENCE,  IN_SPEC_CONFIDENCE
      // OUTPUTS - OUT_DIFF_RADIANCE_HITDIST, OUT_SPEC_RADIANCE_HITDIST

      // from SIGMA_SHADOW_TRANSLUCENCY
      // INPUTS - IN_NORMAL_ROUGHNESS, IN_SHADOWDATA, IN_SHADOW_TRANSLUCENCY, OUT_SHADOW_TRANSLUCENCY (used as history)
      // OUTPUTS - OUT_SHADOW_TRANSLUCENCY

      uint2 textureSize{};

      mat4 mViewToClip{};
      mat4 mWorldToView{};

      mat4 mViewToClipPrev{};
      mat4 mWorldToViewPrev{};

      bool validation = false;
      bool clearHistory = false;
      bool diffuseSpecular = true;
      bool shadow = true;
      bool perfMode = true;
      float splitScreen = 0.f;

      Texture2D* IN_MV{};
      Texture2D* IN_NORMAL_ROUGHNESS{};
      Texture2D* IN_VIEWZ{};
      Texture2D* IN_DIFF_RADIANCE_HITDIST{};
      Texture2D* IN_SPEC_RADIANCE_HITDIST{};

      Texture2D* OUT_DIFF_RADIANCE_HITDIST{};
      Texture2D* OUT_SPEC_RADIANCE_HITDIST{};

      Texture2D* IN_SHADOWDATA{};
      Texture2D* IN_SHADOW_TRANSLUCENCY{};
      Texture2D* OUT_SHADOW_TRANSLUCENCY{};

      Texture2D* OUT_VALIDATION{};
   };

   void NRDDenoise(CommandList& cmd, const DenoiseCallDesc& desc);
   void NRDTerm(); // call on resize too

   // todo:
   void NRDResize(uint2 renderResolution);

}
