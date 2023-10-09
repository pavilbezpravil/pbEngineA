#include "pch.h"
#include "NotifyManager.h"

#include "gui/Gui.h"

namespace pbe {

   void NotifyManager::AddNotify(std::string_view message, Type type) {
      notifies.push_back({ type, message.data() });
   }

   void NotifyManager::UI(float nextNotifyPosY) {
      float notifyWindowPadding = 3;

      UI_PUSH_STYLE_COLOR(ImGuiCol_ChildBg, (ImVec4{ 0, 0, 0, 0.3f }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_ChildRounding, 5);
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ notifyWindowPadding, notifyWindowPadding }));

      float paddingBetweenNotifies = 3;
      nextNotifyPosY -= paddingBetweenNotifies;

      for (auto& notify : notifies | std::views::reverse) {
         notify.time -= ImGui::GetIO().DeltaTime;

         const auto title = notify.title.c_str();
         ImVec2 messageSize = ImGui::CalcTextSize(title) + ImVec2{ notifyWindowPadding, notifyWindowPadding } * 2.f;

         nextNotifyPosY -= messageSize.y + paddingBetweenNotifies;

         float windowSizeX = ImGui::GetWindowSize().x;
         ImGui::SetCursorPos(ImVec2{ windowSizeX - messageSize.x - paddingBetweenNotifies, nextNotifyPosY });

         UI_PUSH_ID(&notify);

         if (UI_CHILD_WINDOW("Notify panel", messageSize, false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
            ImGui::Text(title);
         }
      }

      auto removeRange = std::ranges::remove_if(notifies, [](const Notify& notify) { return notify.time < 0; });
      notifies.erase(removeRange.begin(), removeRange.end());
   }

}
