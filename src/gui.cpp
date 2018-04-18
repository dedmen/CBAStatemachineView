# include "Application.h"
# include <imgui_node_editor.h>
# define IMGUI_DEFINE_MATH_OPERATORS
# include <imgui_internal.h>
#include "statemachine.hpp"

#define FMT_HEADER_ONLY
#include "../fmt/include/fmt/format.h"

namespace ed = ax::NodeEditor;

// Struct to hold basic information about connection between
// pins. Note that connection (aka. link) has its own ID.
// This is useful later with dealing with selections, deletion
// or other operations.
struct LinkInfo
{
    ed::LinkId Id;
    ed::PinId  InputId;
    ed::PinId  OutputId;
};

static ed::EditorContext* g_Context = nullptr;    // Editor context, required to trace a editor state.
static bool                 g_FirstFrame = true;    // Flag set for first frame only, some action need to be executed once.
static ImVector<LinkInfo>   g_Links;                // List of live links. It is dynamic unless you want to create read-only view over nodes.
static int                  g_NextLinkId = 800;     // Counter to help generate link ids. In real application this will probably based on pointer to user data structure.


const char* Application_GetName()
{
    return "Basic Interaction";
}

void Application_Initialize()
{
    ed::Config config;
    config.SettingsFile = "BasicInteraction.json";
    g_Context = ed::CreateEditor(&config);
}

void Application_Finalize()
{
    ed::DestroyEditor(g_Context);
}

void ImGuiEx_BeginColumn()
{
    ImGui::BeginGroup();
}

void ImGuiEx_NextColumn()
{
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
}

void ImGuiEx_EndColumn()
{
    ImGui::EndGroup();
}


void State::renderNode(int& uniqueId, bool isActive) {
    ui_uid = uniqueId++;


    if (isActive)
        ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(228, 128, 128, 200));


    ed::BeginNode(ui_uid);

    ImGui::Text(name.c_str());

    ImGuiEx_BeginColumn();

    for (auto& input : inputs) {

        ed::BeginPin(uniqueId++, ed::PinKind::Input);
        //ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
        ed::PinPivotSize(ImVec2(0, 0));
        ImGui::BeginHorizontal(input.second.c_str());

            ImGui::TextUnformatted(input.second.c_str());
            //ImGui::Spring(0);
        
        //ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
        ImGui::EndHorizontal();
        ed::EndPin();
    }

    ImGuiEx_NextColumn();

    for (auto& output : transitions) {
        output.ui_uid = uniqueId++;
        ed::BeginPin(output.ui_uid, ed::PinKind::Output);

        ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
        ed::PinPivotSize(ImVec2(0, 0));
        //ImGui::BeginHorizontal(output.name.c_str());

            //ImGui::TextUnformatted(output.name.c_str());
            //ImGui::Spring(0);


            if (ImGui::TreeNode(fmt::format("{0}###{1}{2}",output.name, ui_uid, output.ui_uid).c_str())) {
                //ImGui::BeginChild("Scrolling");
                ImGui::TextUnformatted(output.condition.operator r_string().c_str());
                //ImGui::EndChild();
                ImGui::TreePop();
            }

        //ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
        //ImGui::EndHorizontal();
        ed::EndPin();
    }

    for (auto& output : eventTransitions) {
        output.ui_uid = uniqueId++;
        ed::BeginPin(output.ui_uid, ed::PinKind::Input);
        ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
        ed::PinPivotSize(ImVec2(0, 0));
            
        //ImGui::BeginHorizontal(output.name.c_str());

            //ImGui::TextUnformatted(output.name.c_str());
            //ImGui::Spring(0);


            if (ImGui::TreeNode(fmt::format("{0}###{1}{2}", output.name, ui_uid, output.ui_uid).c_str())) {
                ImGuiEx_BeginColumn();
                for (auto& it : output.events) {
                    ImGui::Text(it.c_str());
                }
                ImGuiEx_EndColumn();
                ImGui::TreePop();
            }

        //if (ImGui::CollapsingHeader("Events")) {



           
        //}

        //ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
        //ImGui::EndHorizontal();
        ed::EndPin();




    }

    ImGuiEx_EndColumn();

    ed::EndNode();

    if (isActive)
        ed::PopStyleColor(1);
}


int activeStateMachineIdx = 0;
int activeUnitIdx = 0;

void Application_Frame()
{
    auto& io = ImGui::GetIO();

    ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

    ImGui::Separator();

    std::lock_guard guard(stateMachineMutex);
    if (activeStateMachineIdx >= statemachines.size()) activeStateMachineIdx = 0;
    auto& activeStateMachine = statemachines[activeStateMachineIdx];

    if (ImGui::BeginCombo("StateMachine", fmt::format("{0}", activeStateMachine.id).c_str(), 0)) { // The second parameter is the label previewed before opening the combo.
        int idx = 0;
        for (auto& machine : statemachines) {
            bool is_selected = idx == activeStateMachineIdx;
            if (ImGui::Selectable(fmt::format("{0}", machine.id).c_str(), is_selected)) {
                activeStateMachineIdx = idx;
                activeUnitIdx = 0;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
            idx++;
        }
        ImGui::EndCombo();
    }

    if (!activeStateMachine.units.empty()) {
        if (activeUnitIdx >= activeStateMachine.units.size()) activeUnitIdx = 0;

        if (ImGui::BeginCombo("Unit", activeStateMachine.units[activeUnitIdx].name.c_str(), 0)) { // The second parameter is the label previewed before opening the combo.
            int idx = 0;
            for (auto& unit : activeStateMachine.units) {
                bool is_selected = idx == activeUnitIdx;
                if (ImGui::Selectable(unit.name.c_str(), is_selected))
                    activeUnitIdx = idx;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                idx++;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();

    ed::SetCurrentEditor(g_Context);

    // Start interaction with editor.
    ed::Begin("My Editor", ImVec2(0.0, 0.0f));
  
    int uniqueId = 1;
   

    Unit* activeUnit = nullptr;
    if (activeUnitIdx < activeStateMachine.units.size()) activeUnit = &activeStateMachine.units[activeUnitIdx];


    for (auto& state : activeStateMachine.states) {
        bool isActive = activeUnit && activeUnit->state == state.name;
        state.renderNode(uniqueId, isActive);
    }


    // Submit Links
    //for (auto& linkInfo : g_Links)
    //    ed::Link(linkInfo.Id, linkInfo.InputId, linkInfo.OutputId);

    int nextLinkId = 100;

    auto getStateByName = [&activeStateMachine](std::string_view name) -> State* {
        auto& entry = activeStateMachine.states.get(name);
        if (map_string_to_class<State, auto_array<State>>::is_null(entry)) return {};
        return &entry;
    };

    for (auto& state : activeStateMachine.states) {
        for (auto& transition : state.transitions) {
            auto targetState = getStateByName(transition.targetState);
            if (!targetState) continue;
            auto found = std::find(targetState->inputs.begin(), targetState->inputs.end(), std::pair{ state.name, transition.name });
            if (found == targetState->inputs.end()) continue;
            auto outputID = targetState->ui_uid + 1 + std::distance(targetState->inputs.begin(), found);
            ed::Link(nextLinkId++, transition.ui_uid, outputID);
            if (transition.active)
                ed::Flow(nextLinkId - 1);
            transition.active = false;
        }

        for (auto& transition : state.eventTransitions) {
            auto targetState = getStateByName(transition.targetState);
            if (!targetState) continue;
            auto found = std::find(targetState->inputs.begin(), targetState->inputs.end(), std::pair{ state.name, transition.name });
            if (found == targetState->inputs.end()) continue;
            auto outputID = targetState->ui_uid + 1 + targetState->transitions.size() + std::distance(targetState->inputs.begin(), found);
            ed::Link(nextLinkId++, transition.ui_uid, outputID);
            if (transition.active)
                ed::Flow(nextLinkId - 1);
            transition.active = false;
        }
    }

    //
    // 2) Handle interactions
    //

    // Handle creation action, returns true if editor want to create new object (node or link)
    if (ed::BeginCreate())
    {
        ed::PinId inputPinId, outputPinId;
        if (ed::QueryNewLink(&inputPinId, &outputPinId))
        {
            // QueryNewLink returns true if editor want to create new link between pins.
            //
            // Link can be created only for two valid pins, it is up to you to
            // validate if connection make sense. Editor is happy to make any.
            //
            // Link always goes from input to output. User may choose to drag
            // link from output pin or input pin. This determine which pin ids
            // are valid and which are not:
            //   * input valid, output invalid - user started to drag new ling from input pin
            //   * input invalid, output valid - user started to drag new ling from output pin
            //   * input valid, output valid   - user dragged link over other pin, can be validated

            if (inputPinId && outputPinId) // both are valid, let's accept link
            {
                // ed::AcceptNewItem() return true when user release mouse button.
                if (ed::AcceptNewItem())
                {
                    // Since we accepted new link, lets add one to our list of links.
                    g_Links.push_back({ ed::LinkId(g_NextLinkId++), inputPinId, outputPinId });

                    // Draw new link.
                    ed::Link(g_Links.back().Id, g_Links.back().InputId, g_Links.back().OutputId);
                }

                // You may choose to reject connection between these nodes
                // by calling ed::RejectNewItem(). This will allow editor to give
                // visual feedback by changing link thickness and color.
            }
        }
    }
    ed::EndCreate(); // Wraps up object creation action handling.


    // Handle deletion action
    if (ed::BeginDelete())
    {
        // There may be many links marked for deletion, let's loop over them.
        ed::LinkId deletedLinkId;
        while (ed::QueryDeletedLink(&deletedLinkId))
        {
            // If you agree that link can be deleted, accept deletion.
            if (ed::AcceptDeletedItem())
            {
                // Then remove link from your data.
                for (auto& link : g_Links)
                {
                    if (link.Id == deletedLinkId)
                    {
                        g_Links.erase(&link);
                        break;
                    }
                }
            }

            // You may reject link deletion by calling:
            // ed::RejectDeletedItem();
        }
    }
    ed::EndDelete(); // Wrap up deletion action

    // End of interaction with editor.
    ed::End();

    if (g_FirstFrame)
        ed::NavigateToContent(0.0f);

    ed::SetCurrentEditor(nullptr);

    g_FirstFrame = false;

    // ImGui::ShowMetricsWindow();
}

