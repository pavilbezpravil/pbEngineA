#pragma once

#include "Types.h"

namespace pbe {

   // using Color = vec4;

   struct Color : vec4 {
      using vec4::vec4;

      constexpr Color(float r, float g, float b) : vec4(r, g, b, 1.0f) {}
      constexpr Color(vec3 rgb) : Color(rgb.r, rgb.g, rgb.b) {}

      constexpr vec3 Rgb() const { return {r, g, b}; }
      constexpr operator vec3() const { return Rgb(); }
   };

   constexpr Color Color_Black = Color(0, 0, 0);
   constexpr Color Color_Gray = Color(0.5f, 0.5f, 0.5f);
   constexpr Color Color_White = Color(1, 1, 1);

   constexpr Color Color_Red = Color(1, 0, 0);
   constexpr Color Color_Green = Color(0, 1, 0);
   constexpr Color Color_Blue = Color(0, 0, 1);

   constexpr Color Color_Yellow = Color(1, 1, 0);
   constexpr Color Color_Cyan = Color(0, 1, 1);
   constexpr Color Color_Magenta = Color(1, 0, 1);

   constexpr Color Color_Orange = Color(1, 0.5f, 0);
   constexpr Color Color_Pink = Color(1, 0, 0.5f);
   constexpr Color Color_Lime = Color(0.5f, 1, 0);
   constexpr Color Color_Teal = Color(0, 1, 0.5f);
   constexpr Color Color_Purple = Color(0.5f, 0, 1);
   constexpr Color Color_Navy = Color(0, 0.5f, 1);

}
