#pragma once

#include <string_view>

#include "core/Assert.h"
#include "core/Core.h"
#include "core/Type.h"

namespace pbe {

   // todo:
   constexpr char DRAG_DROP_ENTITY[] = "DD_ENTITY";

   namespace ui {

      struct Window {
         Window(const char* name, bool* p_open = NULL, ImGuiWindowFlags flags = 0) {
            opened = ImGui::Begin(name, p_open, flags);
         }

         ~Window() {
            ImGui::End();
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

#define UI_WINDOW(name, ...) ui::Window uiWindow{name, __VA_ARGS__}

      struct MenuBar {
         MenuBar() {
            opened = ImGui::BeginMenuBar();
         }

         ~MenuBar() {
            ImGui::EndMenuBar();
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

#define UI_MENU_BAR() ui::MenuBar uiMenuBar{}

      struct Menu {
         Menu(const char* name, bool enabled = true) {
            opened = ImGui::BeginMenu(name, enabled);
         }

         ~Menu() {
            if (opened) {
               ImGui::End();
            }
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

      // name, enabled
#define UI_MENU(name, ...) ui::Menu uiMenu{name, __VA_ARGS__}

      struct PushID {
         PushID(const void* ptrID) {
            ImGui::PushID(ptrID);
         }

         PushID(int intID) {
            ImGui::PushID(intID);
         }

         PushID(const char* strID) {
            ImGui::PushID(strID);
         }

         PushID(const char* strID_begin, const char* strID_end) {
            ImGui::PushID(strID_begin, strID_end);
         }

         ~PushID() {
            ImGui::PopID();
         }
      };

#define UI_PUSH_ID(id) ui::PushID uiPushID{id}

      struct TreeNode {
         TreeNode(const char* name, ImGuiTreeNodeFlags flags = 0) {
            opened = ImGui::TreeNodeEx(name, flags);
         }

         template<typename... Args>
         TreeNode(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, Args... args) {
            opened = ImGui::TreeNodeEx(str_id, flags, fmt, std::forward<Args>(args)...);
         }

         template<typename... Args>
         TreeNode(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, Args... args) {
            opened = ImGui::TreeNodeEx(ptr_id, flags, fmt, std::forward<Args>(args)...);
         }

         ~TreeNode() {
            if (opened) {
               ImGui::TreePop();
            }
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

#define UI_TREE_NODE(Name_StrId_PtrId, ...) ui::TreeNode uiTreeNode{Name_StrId_PtrId, __VA_ARGS__}

      struct PopupContextX {
         PopupContextX(int type, const char* name = nullptr, ImGuiPopupFlags flags = 1) {
            switch (type) {
               case 1:
                  opened = ImGui::BeginPopupContextItem(name, flags);
                  break;
               case 2:
                  opened = ImGui::BeginPopupContextWindow(name, flags);
                  break;
               case 3:
                  opened = ImGui::BeginPopupContextVoid(name, flags);
                  break;
               default:
                  UNIMPLEMENTED();
                  break;
            }
         }

         ~PopupContextX() {
            if (opened) {
               ImGui::EndPopup();
            }
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

#define UI_POPUP_CONTEXT_ITEM(...) ui::PopupContextX uiPopupContextItem{1, __VA_ARGS__}
#define UI_POPUP_CONTEXT_WINDOW(...) ui::PopupContextX uiPopupContextWindow{2, __VA_ARGS__}
#define UI_POPUP_CONTEXT_VOID(...) ui::PopupContextX uiPopupContextVoid{3, __VA_ARGS__}

      struct DragDropSource {
         template<typename T>
         DragDropSource(const char* type, const T& data, ImGuiCond cond = ImGuiCond_None, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None) {
            opened = ImGui::BeginDragDropSource(flags);
            if (opened) {
               ImGui::SetDragDropPayload(type, &data, sizeof(T), cond);
            }
         }

         ~DragDropSource() {
            if (opened) {
               ImGui::EndDragDropSource();
            }
         }

         operator bool() const { return opened; }

         bool opened = false;
      };

#define UI_DRAG_DROP_SOURCE(type, data, ...) ui::DragDropSource uiDragDropSource{type, data, __VA_ARGS__}

      struct DragDropTarget {
         DragDropTarget(const char* type, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None) {
            if (opened = ImGui::BeginDragDropTarget()) {
               payload = ImGui::AcceptDragDropPayload(type, flags);
            }
         }

         template<typename T>
         T* GetPayload() {
            ASSERT(payload->DataSize == sizeof(T));
            return (T*)payload->Data;
         }

         ~DragDropTarget() {
            if (opened) {
               ImGui::EndDragDropTarget();
            }
         }

         operator bool() const { return opened && payload; }

         bool opened = false;
         const ImGuiPayload* payload = nullptr;
      };

      // ui::DragDropTarget ddTarget{ type }
#define UI_DRAG_DROP_TARGET(type, ...) ui::DragDropTarget ddTarget{type, __VA_ARGS__}
   }

   CORE_API ImGuiContext* GetImGuiContext();

   CORE_API bool EditorUI(std::string_view name, TypeID typeID, byte* value);

   template<typename T>
   bool EditorUI(std::string_view name, T& value) {
      const auto typeID = GetTypeID<T>();
      return EditorUI(name, typeID, (byte*)&value);
   }

   bool UIColorEdit3(const char* name, byte* value);
   bool UIColorPicker3(const char* name, byte* value);

   struct UISliderFloat {
      float min = 0;
      float max = 1;

      bool operator()(const char* name, byte* value) const{
         return ImGui::SliderFloat(name, (float*)value, min, max);
      }
   };

}
