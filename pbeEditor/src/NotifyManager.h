#pragma once

namespace pbe {

   class NotifyManager {
   public:
      enum Type {
         Info,
         Warning,
         Error,
      };

      void AddNotify(std::string_view message, Type type = Info);
      void UI(float nextNotifyPosY);

   private:
      struct Notify {
         Type type;
         std::string title;
         float time = 3.f;
      };

      std::vector<Notify> notifies;
   };

}
