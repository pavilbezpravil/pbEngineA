#pragma once

namespace pbe {

   class StringID {
   public:
      StringID() = default;
      StringID(std::string_view str);

      int GetID() const { return id; }

   private:
      int id = -1;
   };

}
