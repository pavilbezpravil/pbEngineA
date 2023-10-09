#include "pch.h"
#include "NotifyManager.h"

#include "gui/Gui.h"

namespace pbe {

   void NotifyManager::AddNotify(std::string_view message, Type type) {
      notifies.push_back({ type, message.data() });
   }

   void NotifyManager::UI() {
      float notifyWindowPadding = 5;

      UI_PUSH_STYLE_COLOR(ImGuiCol_ChildBg, (ImVec4{ 0, 0, 0, 0.3f }));
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_ChildRounding, 10);
      UI_PUSH_STYLE_VAR(ImGuiStyleVar_WindowPadding, (ImVec2{ notifyWindowPadding, notifyWindowPadding }));

      ImGui::SetCursorPos({ 0, 0 });
      auto contentRegion = ImGui::GetContentRegionAvail();

      float paddingBetweenNotifies = 5;
      float nextNotifyPosY = contentRegion.y - paddingBetweenNotifies;

      for (auto [i, notify] : std::views::enumerate(notifies)) {
         notify.time -= ImGui::GetIO().DeltaTime;

         const auto title = notify.title.c_str();
         ImVec2 messageSize = ImGui::CalcTextSize(title) + ImVec2{ notifyWindowPadding, notifyWindowPadding } * 2.f;

         nextNotifyPosY -= messageSize.y + paddingBetweenNotifies;
         ImGui::SetCursorPos(ImVec2{ paddingBetweenNotifies, nextNotifyPosY });

         UI_PUSH_ID((int)i);

         if (UI_CHILD_WINDOW("Notify panel", messageSize, false, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(title);
         }
      }

      auto removeRange = std::ranges::remove_if(notifies, [](const Notify& notify) { return notify.time < 0; });
      notifies.erase(removeRange.begin(), removeRange.end());
   }

}
