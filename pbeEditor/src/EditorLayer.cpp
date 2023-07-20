#include "pch.h"
#include "EditorLayer.h"
#include "EditorWindow.h"
#include "Undo.h"
#include "ViewportWindow.h"
#include "app/Event.h"
#include "app/Input.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "core/Type.h"
#include "fs/FileSystem.h"
#include "gui/Gui.h"
#include "math/Random.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "typer/Serialize.h"
#include "typer/Typer.h"


namespace pbe {

   constexpr char editorSettingPath[] = "editor.yaml";

   TYPER_BEGIN(EditorSettings)
      TYPER_FIELD(scenePath)
      TYPER_FIELD(cameraPos)
   TYPER_END()

   Entity CreateEmpty(Scene& scene, string_view namePrefix = "Empty", Entity parent = {}, const vec3& pos = {}) {
      // todo: find appropriate name
      auto entity = scene.Create(parent, namePrefix);
      entity.Get<SceneTransformComponent>().position = pos;
      return entity;
   }

   struct CubeDesc {
      Entity parent = {};
      string_view namePrefix = "Cube";
      vec3 pos{};
      vec3 scale = vec3_One;
      quat rotation = quat_Identity;
      bool dynamic = true;
      vec3 color = vec3_One * 0.7f;
   };

   Entity CreateCube(Scene& scene, const CubeDesc& desc = {}) {
      auto entity = CreateEmpty(scene, desc.namePrefix, desc.parent, desc.pos);

      auto& trans = entity.Get<SceneTransformComponent>();
      trans.scale = desc.scale;
      trans.rotation = desc.rotation;

      entity.Add<GeometryComponent>();
      entity.Add<SimpleMaterialComponent>().baseColor = desc.color;

      // todo:
      RigidBodyComponent _rb{};
      _rb.dynamic = desc.dynamic;
      entity.Add<RigidBodyComponent>(_rb);
      
      return entity;
   }

   Entity CreateDirectLight(Scene& scene, string_view namePrefix = "Direct Light", const vec3& pos = {}) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Get<SceneTransformComponent>().rotation = quat{ vec3{PIHalf * 0.5, PIHalf * 0.5, 0 } };
      entity.Add<DirectLightComponent>();
      return entity;
   }

   Entity CreateSky(Scene& scene, string_view namePrefix = "Sky", const vec3& pos = {}) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Add<SkyComponent>();
      return entity;
   }

   class SceneHierarchyWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void DragDropChangeParent(Entity entity = {}) {
         if (ui::DragDropTarget ddTarget{ DRAG_DROP_ENTITY}) {
            auto childEnt = *ddTarget.GetPayload<Entity>();
            Undo::Get().SaveForFuture(childEnt); // todo: dont work
            bool changed = childEnt.Get<SceneTransformComponent>().SetParent(entity); // todo: add to pending not in all case
            if (changed) {
               Undo::Get().PushSave();
            }
         }
      }

      void OnImGuiRender() override {
         UI_WINDOW(name.c_str(), &show);

         if (!pScene) {
            ImGui::Text("No scene");
         } else {
            // todo: undo on delete entity

            // todo: undo
            if (UI_POPUP_CONTEXT_WINDOW(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
               if (ImGui::MenuItem("Create Empty")) {
                  ToggleSelectEntity(CreateEmpty(*pScene));
               }
               if (ImGui::MenuItem("Create Dynamic Cube")) {
                  ToggleSelectEntity(CreateCube(*pScene));
               }
               if (ImGui::MenuItem("Create Static Cube")) {
                  ToggleSelectEntity(CreateCube(*pScene, CubeDesc{ .dynamic = false }));
               }
               if (ImGui::MenuItem("Create Direct Light")) {
                  ToggleSelectEntity(CreateDirectLight(*pScene));
               }
               if (ImGui::MenuItem("Create Sky")) {
                  ToggleSelectEntity(CreateSky(*pScene));
               }
               if (ImGui::MenuItem("Create Decal")) {
                  auto createdEntity = pScene->Create();
                  createdEntity.Add<DecalComponent>();
                  ToggleSelectEntity(createdEntity);
               }
            }

            UIEntity(pScene->GetRootEntity(), true);

            // place item for drag&drop to root
            // ImGui::SetCursorPosY(0); // todo: mb it help to reorder items in hierarchy
            ImGui::Dummy(ImGui::GetContentRegionAvail());
            DragDropChangeParent();
            if (ImGui::IsItemClicked() && !Input::IsKeyPressed(VK_CONTROL)) {
               selection->ClearSelection();
            }
         }
      }

      void UIEntity(Entity entity, bool sceneRoot = false) {
         auto& trans = entity.Get<SceneTransformComponent>();
         bool hasChilds = trans.HasChilds();

         const auto* name = std::invoke([&] {
            auto c = entity.TryGet<TagComponent>();
            return c ? c->tag.data() : "Unnamed Entity";
            });

         auto entityUIAction = [&]() {
            if (sceneRoot) {
               return;
            }

            if (ImGui::IsItemHovered()) {
               ImGui::SetTooltip("Right-click to open popup %s", name);
            }

            if (UI_DRAG_DROP_SOURCE(DRAG_DROP_ENTITY, entity)) {
               ImGui::Text("Drag&Drop Entity %s", name);
            }

            DragDropChangeParent(entity);

            // IsItemToggledOpen: when node opened dont select entity
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
               ToggleSelectEntity(entity);
            }

            if (UI_POPUP_CONTEXT_ITEM()) {
               ImGui::Text("This a popup for \"%s\"!", name);

               ImGui::Separator();

               if (ImGui::Button("Delete")) {
                  // todo: add to pending
                  Undo::Get().Delete(entity);
                  if (selection) {
                     selection->Unselect(entity);
                  }
                  pScene->DestroyImmediate(entity);
               }

               if (ImGui::Button("Unparent")) {
                  trans.SetParent();
               }

               if (ImGui::Button("Close")) {
                  ImGui::CloseCurrentPopup();
               }

               if (ImGui::Button("Save to file")) {
                  Serializer ser;
                  EntitySerialize(ser, entity);
                  // ser.SaveToFile(std::format("{}.yaml", entity.GetName()));
                  ser.SaveToFile("entity.yaml");
               }
               if (ImGui::Button("Load from file")) {
                  auto deser = Deserializer::FromFile("entity.yaml");
                  EntityDeserialize(deser, *entity.GetScene());
               }
            }
         };

         ImGuiTreeNodeFlags nodeFlags =
            (selection->IsSelected(entity) ? ImGuiTreeNodeFlags_Selected : 0)
            | (sceneRoot ? ImGuiTreeNodeFlags_DefaultOpen : 0)
            | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanFullWidth
            | (hasChilds ? 0 : ImGuiTreeNodeFlags_Leaf);

         if (UI_TREE_NODE((void*)(uint64)entity.GetUUID(), nodeFlags, name)) {
            entityUIAction();

            for (auto child : trans.children) {
               UIEntity(child);
            }
         } else {
            entityUIAction();
         }
      }

      void SetScene(Scene* scene) {
         pScene = scene;
      }

      void ToggleSelectEntity(Entity entity) {
         if (selection) {
            bool clearPrevSelection = !Input::IsKeyPressed(VK_CONTROL);
            selection->ToggleSelect(entity, clearPrevSelection);
         }
      }

      Scene* pScene{};
      EditorSelection* selection{};
   };

   // todo: move to other file
   template<typename T>
   struct Component {
      T* operator->() { return entity.TryGet<T>(); }
      const T* operator->() const { return entity.TryGet<T>(); }

      Entity entity;
   };

   class InspectorWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         UI_WINDOW(name.c_str(), &show);

         Entity entity = selection ? selection->FirstSelected() : Entity{};

         if (!entity.Valid()) {
            ImGui::Text("No entity");
         } else {
            Undo::Get().SaveForFuture(entity);

            bool edited = false;

            // ImGui::Text("%s %llu", entity.Get<TagComponent>().tag.c_str(), (uint64)entity.Get<UUIDComponent>().uuid);

            std::string name = entity.Get<TagComponent>().tag;
            name.reserve(glm::max((int)name.size(), 64));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
            if (ImGui::InputText("Name", name.data(), name.capacity())) {
               entity.Get<TagComponent>().tag = name.c_str();
               edited = true;
            }

            ImGui::SameLine();
            ImGui::Text("0x%jx", (uint64)entity.Get<UUIDComponent>().uuid);

            edited |= EditorUI("Scene Transform", entity.GetTransform());

            const auto& typer = Typer::Get();

            for (const auto& ci : typer.components) {
               if (auto* pComponent = ci.tryGet(entity)) {
                  const auto& ti = typer.GetTypeInfo(ci.typeID);
                  edited |= EditorUI(ti.name.data(), ci.typeID, (byte*)pComponent);
               }
            }

            if (UI_POPUP_CONTEXT_ITEM("Add Component Popup")) {
               for (const auto& ci : typer.components) {
                  auto* pComponent = ci.tryGet(entity);
                  if (!pComponent) {
                     auto text = std::format("Add {}", typer.GetTypeInfo(ci.typeID).name);
                     if (ImGui::Button(text.data())) {
                        ci.getOrAdd(entity);
                        edited = true;
                     }
                  }
               }
            }

            if (ImGui::Button("Add Component")) {
               ImGui::OpenPopup("Add Component Popup");
            }

            if (edited) {
               Undo::Get().PushSave();
            }
         }
      }
      
      EditorSelection* selection{};
   };

   class ProfilerWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         auto& profiler = Profiler::Get();

         ImGui::Text("Profiler Stats");
         ImGui::Text("Cpu:");
         for (const auto& [name, cpuEvent] : profiler.cpuEvents) {
            if (!cpuEvent.usedInFrame) {
               continue;
            }
            // ImGui::Text("  %s: %.2f %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage(), cpuEvent.averageTime.GetCur());
            ImGui::Text("  %s: %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage());
         }

         ImGui::Text("Gpu:");
         for (const auto& [name, gpuEvent] : profiler.gpuEvents) {
            if (!gpuEvent.usedInFrame) {
               continue;
            }
            // ImGui::Text("  %s: %.2f %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage(), gpuEvent.averageTime.GetCur());
            ImGui::Text("  %s: %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage());
         }

         ImGui::End();
      }
   };

   class ConfigVarsWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         const ConfigVarsMng& configVars = sConfigVarsMng;

         ImGui::Text("Vars:");

         CVarChilds(configVars.GetCVarsRoot());

         ImGui::End();
      }

      void CVarChilds(const ConfigVarsMng::CVarChilds& childs) {
         if (childs.cvar) {
            childs.cvar->UI();
         } else {
            for (const auto &child : childs.childs) {
               if (child.IsLeaf()) {
                  CVarChilds(child);
               } else {
                  if (ImGui::TreeNodeEx(child.folderName.c_str())) {
                     CVarChilds(child);
                     ImGui::TreePop();
                  }
               }
            }
         }
      }
   };

   class ShaderWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);
         ShadersWindow();
         ImGui::End();
      }
   };

   void EditorLayer::OnAttach() {
      Deserialize(editorSettingPath, editorSettings);

      ImGui::SetCurrentContext(GetImGuiContext());

      ReloadDll();

      AddEditorWindow(new ConfigVarsWindow("ConfigVars"), true);
      AddEditorWindow(sceneHierarchyWindow = new SceneHierarchyWindow("SceneHierarchy"), true);
      AddEditorWindow(inspectorWindow = new InspectorWindow("Inspector"), true);
      AddEditorWindow(viewportWindow = new ViewportWindow("Viewport"), true);
      AddEditorWindow(new ProfilerWindow("Profiler"), true);
      AddEditorWindow(new ShaderWindow("Shader"), true);

      sceneHierarchyWindow->selection = &editorSelection;
      viewportWindow->selection = &editorSelection;
      inspectorWindow->selection = &editorSelection;

      viewportWindow->customHeadFunc = [&]() {
         switch (editorState) {
            case State::Edit:
               if (ImGui::Button("Play") && editorScene) {
                  runtimeScene = editorScene->Copy();
                  SetActiveScene(runtimeScene.get());
                  runtimeScene->OnStart();
                  editorState = State::Play;
               }
               break;
            case State::Play:
               if (ImGui::Button("Stop")) {
                  runtimeScene->OnStop();
                  runtimeScene = {};
                  SetActiveScene(editorScene.get());
                  editorState = State::Edit;
               }
               break;
            default: UNIMPLEMENTED();
         }

         if (editorState == State::Edit) {
            ImGui::SameLine();
            if (ImGui::Button("Reload dll")) {
               ReloadDll();
            }
         }
      };

      if (!editorSettings.scenePath.empty()) {
         auto s = SceneDeserialize(editorSettings.scenePath);
         SetEditorScene(std::move(s));
      }

      Layer::OnAttach();
   }

   void EditorLayer::OnDetach() {
      ImGui::SetCurrentContext(nullptr);

      runtimeScene = {};
      editorScene = {};
      UnloadDll();

      Serialize(editorSettingPath, editorSettings);

      Layer::OnDetach();
   }

   void EditorLayer::OnUpdate(float dt) {
      for (auto& window : editorWindows) {
         if (window->show) {
            window->OnUpdate(dt);
         }
      }

      if (editorState == State::Play && runtimeScene) {
         runtimeScene->OnUpdate(dt);
      }
   }

   void EditorLayer::OnImGuiRender() {
      static bool p_open = true; // todo:

      static bool opt_fullscreen = true;
      static bool opt_padding = false;
      static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

      // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
      // because it would be confusing to have two docking targets within each others.
      ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
      if (opt_fullscreen) {
         const ImGuiViewport* viewport = ImGui::GetMainViewport();
         ImGui::SetNextWindowPos(viewport->WorkPos);
         ImGui::SetNextWindowSize(viewport->WorkSize);
         ImGui::SetNextWindowViewport(viewport->ID);
         ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
         ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
         window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove;
         window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
      }
      else {
         dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
      }

      // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
      // and handle the pass-thru hole, so we ask Begin() to not render a background.
      if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
         window_flags |= ImGuiWindowFlags_NoBackground;

      // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
      // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
      // all active windows docked into it will lose their parent and become undocked.
      // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
      // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
      if (!opt_padding)
         ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      ImGui::Begin("DockSpace Demo", &p_open, window_flags);
      if (!opt_padding)
         ImGui::PopStyleVar();

      if (opt_fullscreen)
         ImGui::PopStyleVar(2);

      // Submit the DockSpace
      ImGuiIO& io = ImGui::GetIO();
      if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
         ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
         ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
      }

      if (ImGui::BeginMenuBar()) {
         bool canChangeScene = !runtimeScene;

         if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", nullptr, false, canChangeScene)) {
               editorSettings.scenePath = {};

               Own<Scene> scene = std::make_unique<Scene>();
               CreateDirectLight(*scene);
               CreateSky(*scene);
               CreateCube(*scene, CubeDesc{ .namePrefix = "Ground", .pos = vec3{0, -0.5, 0},
                  .scale = vec3{50, 1, 50}, .dynamic = false, .color = vec3{0.2, 0.6, 0.2} });
               CreateCube(*scene, CubeDesc{ .namePrefix = "Cube", .pos = vec3{0, 3, 0} });

               SetEditorScene(std::move(scene));
            }

            if (ImGui::MenuItem("Open Scene", nullptr, false, canChangeScene)) {
               auto path = OpenFileDialog({"Scene", "*.scn"});
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  auto s = SceneDeserialize(editorSettings.scenePath);
                  SetEditorScene(std::move(s));
               }
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false,
               canChangeScene && !editorSettings.scenePath.empty())) {
               if (editorScene) {
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }

            if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false,
               canChangeScene && !!editorScene)) {
               auto path = OpenFileDialog({ "Scene", "*.scn", true });
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }

            ImGui::Separator();

            auto scene = GetActiveScene();

            vec3 cubeSize{ 25, 10, 25 };

            if (ImGui::MenuItem("Random Cubes", nullptr, false, !!GetActiveScene())) {
               Entity root = scene->Create("Random Cubes");
               editorSelection.ToggleSelect(root);

               for (int i = 0; i < 500; ++i) {
                  CreateCube(*scene, CubeDesc{
                     .parent = root,
                     .namePrefix = std::format("Cube {}", i),
                     .pos = Random::Float3(-cubeSize, cubeSize),
                     .scale = Random::Float3(vec3{ 0 }, vec3{ 3.f }),
                     .rotation = Random::Float3(vec3{ 0 }, vec3{ 30.f }), // todo: PI?
                     .color = Random::Color() });
               }
            }

            if (ImGui::MenuItem("Random Lights", nullptr, false, !!GetActiveScene())) {
               Entity root = scene->Create("Random Lights");
               editorSelection.ToggleSelect(root);

               for (int i = 0; i < 8; ++i) {
                  Entity e = CreateEmpty(*scene, std::format("Light {}", i),
                     root, Random::Float3(-cubeSize, cubeSize));

                  auto& light = e.Add<LightComponent>();
                  light.color = Random::Float3(vec3{ 0 }, vec3{ 1.f });
                  light.intensity = Random::Float(0.f, 20.f);
                  light.radius = Random::Float(3, 10);
               }
            }

            if (ImGui::MenuItem("Add Geom if Material is presents", nullptr, false, !!GetActiveScene())) {
               auto view = scene->View<SimpleMaterialComponent>();
               for (auto _e : view) {
                  Entity e{ _e, scene };
                  auto& geom = e.GetOrAdd<GeometryComponent>();
                  geom.type = GeomType::Box;
               }
            }

            if (ImGui::MenuItem("Create wall", nullptr, false, !!GetActiveScene())) {
               Entity root = scene->Create("Wall");
               editorSelection.ToggleSelect(root);

               int size = 5;
               for (int y = 0; y < size; ++y) {
                  for (int x = 0; x < size; ++x) {
                     CreateCube(*scene, CubeDesc{
                        .parent = root,
                        .pos = vec3{ -size / 2.f + x, y + 0.5f, 0 },
                        .color = Random::Color() });
                  }
               }
            }

            if (ImGui::MenuItem("Create stack", nullptr, false, !!GetActiveScene())) {
               Entity root = scene->Create("Stack");
               editorSelection.ToggleSelect(root);

               int size = 10;
               for (int i = 0; i < size; ++i) {
                  CreateCube(*scene, CubeDesc{
                     .parent = root,
                     .pos = vec3{0, i + 0.5f, 0},
                     .color = Random::Color() });
               }
            }

            if (ImGui::MenuItem("Create stack tri", nullptr, false, !!GetActiveScene())) {
               Entity root = scene->Create("Stack tri");
               editorSelection.ToggleSelect(root);

               int size = 10;
               for (int y = 0; y < size; ++y) {
                  int width = size - y;
                  for (int x = 0; x < width; ++x) {
                     CreateCube(*scene, CubeDesc{
                        .parent = root,
                        .pos = vec3{ -width / 2.f + x, y + 0.5f, 0 },
                        .color = Random::Color() });
                  }
               }
            }

            ImGui::EndMenu();
         }

         ImGui::EndMenuBar();
      }

      if (ImGui::BeginMenuBar()) {
         if (ImGui::BeginMenu("Windows")) {
            for (auto& window : editorWindows) {
               ImGui::MenuItem(window->name.c_str(), NULL, &window->show);
            }
            ImGui::EndMenu();
         }

         ImGui::EndMenuBar();
      }

      ImGui::End();


      for (auto& window : editorWindows) {
         if (window->show) {
            window->OnImGuiRender();
         }
      }
   }

   void EditorLayer::OnEvent(Event& event) {
      if (auto* e = event.GetEvent<KeyPressedEvent>()) {
         // INFO("KeyCode {}", e->keyCode);

         if (e->keyCode == VK_ESCAPE) {
            editorSelection.ClearSelection();
         }
         if (e->keyCode == VK_DELETE) {
            for (auto entity : editorSelection.selected) {
               Undo::Get().Delete(entity); // todo: undo not each but all at once
               GetActiveScene()->DestroyImmediate(entity);
            }
            editorSelection.ClearSelection();
         }
         if (e->keyCode == 'D' && Input::IsKeyPressed(VK_CONTROL)) {
            auto prevSelected = editorSelection.selected;
            editorSelection.ClearSelection();

            for (auto entity : prevSelected) {
               auto duplicatedEntity = editorScene->Duplicate(entity);
               editorSelection.Select(duplicatedEntity, false);
            }
         }

         if (e->keyCode == 'Z' && Input::IsKeyPressed(VK_CONTROL)) {
            // todo: dont work
            // Undo::Get().PopAction();
         }
      }
   }

   void EditorLayer::AddEditorWindow(EditorWindow* window, bool showed) {
      window->show = showed;
      editorWindows.emplace_back(window);
   }

   void EditorLayer::SetEditorScene(Own<Scene>&& scene) {
      editorScene = std::move(scene);
      SetActiveScene(editorScene.get());
   }

   void EditorLayer::SetActiveScene(Scene* scene) {
      editorSelection.ClearSelection();

      sceneHierarchyWindow->SetScene(scene);
      viewportWindow->scene = scene;
   }

   Scene* EditorLayer::GetActiveScene() {
      switch (editorState) {
      case State::Edit:
         return editorScene.get();
      case State::Play:
         return runtimeScene.get();
      default: UNIMPLEMENTED();
      }
      return nullptr;
   }

   void EditorLayer::ReloadDll() {
      auto loadDll = [&] {
         ASSERT(dllHandler == 0);

         std::wstring dllName = L"testProj.dll"; // todo:

         if (1) {
            // todo: for hot reload dll. windows lock dll for writing
            fs::copy_file(dllName, "testProjCopy.dll", std::filesystem::copy_options::update_existing);
            dllHandler = LoadLibrary(L"testProjCopy.dll");
         } else {
            dllHandler = LoadLibrary(dllName.data());
         }

         if (!dllHandler) {
            WARN("could not load the dynamic library testProj.dll");
         }
      };

      if (dllHandler) {
         if (editorScene) {
            // todo: do it on RAM
            INFO("Serialize editor scene for dll reload");
            SceneSerialize("dllReload.scn", *editorScene);

            UnloadDll();
            loadDll();

            INFO("Deserialize editor scene for dll reload");
            auto reloadedScene = SceneDeserialize("dllReload.scn");
            SetEditorScene(std::move(reloadedScene));
         } else {
            UnloadDll();
            loadDll();
         }
      } else {
         loadDll();
      }
   }

   void EditorLayer::UnloadDll() {
      if (dllHandler) {
         INFO("Unload game dll");
         FreeLibrary(dllHandler);
         dllHandler = 0;
      }
   }

}
