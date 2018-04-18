#include <intercept.hpp>
#include "statemachine.hpp"
#define FMT_HEADER_ONLY
#include "../fmt/include/fmt/format.h"

using namespace intercept;
extern int StartUI();
bool UIOpen = false;

std::vector<Statemachine> statemachines;
std::mutex stateMachineMutex;


void launchUI() {
    if (UIOpen) return;
    UIOpen = true;
    std::thread(StartUI).detach();
}


int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

void intercept::register_interfaces() {
    
}

extern int activeStateMachineIdx;
extern int activeUnitIdx;

void intercept::pre_start() {


    static auto fncTransition = client::host::register_sqf_command("cba_sm_trans"sv, ""sv, [](game_state& state) -> game_value {
        std::lock_guard guard(stateMachineMutex);
        int stateMachineID = state.get_local_variable("_id"sv);
        if (statemachines.size() <= activeStateMachineIdx || statemachines[activeStateMachineIdx].id != stateMachineID) return {};
        Statemachine& stateMachine = statemachines[activeStateMachineIdx];

        object unit = state.get_local_variable("_this"sv);

        if (stateMachine.units.size() <= activeUnitIdx || stateMachine.units[activeUnitIdx].unit != unit) return {};

        r_string originStateName = state.get_local_variable("_thisstate"sv);
        auto& originState = stateMachine.states.get(originStateName);
        if (stateMachine.states.is_null(originState)) return {};
        r_string transitionName = state.get_local_variable("_thistransition"sv);
        auto transitionIt = std::find_if(originState.transitions.begin(), originState.transitions.end(), [&transitionName](const Transition& t) {
                return t.name == transitionName;
        });
        if (transitionIt != originState.transitions.end()) {
            transitionIt->active = true;
            return {};
        }

        auto eTransitionIt = std::find_if(originState.eventTransitions.begin(), originState.eventTransitions.end(), [&transitionName](const EventTransition& t) {
                return t.name == transitionName;
            });
        if (eTransitionIt != originState.eventTransitions.end()) {
            eTransitionIt->active = true;
            return {};
        };

        return {};
    }, game_data_type::NOTHING);
}

void codeAddTransitionHook(game_value& val) {
    auto code = val.get_as<game_data_code>();
    auto newCodeGV = sqf::compile("cba_sm_trans;\n" + code->code_string);
    auto newCode = newCodeGV.get_as<game_data_code>();

    code->instructions = newCode->instructions;
}

void refreshStatemachines() {
    auto stateMachines = sqf::get_variable(sqf::mission_namespace(), "cba_statemachine_stateMachines"sv).to_array();
    std::lock_guard guard(stateMachineMutex);
    statemachines.clear();


    for (location machine : stateMachines) {
        Statemachine newMachine;
        newMachine.machine = machine;
        newMachine.id = sqf::get_variable(machine, "cba_statemachine_id"sv);
        auto states = sqf::get_variable(machine, "cba_statemachine_states"sv).to_array();
        for (r_string state : states) {
            State newState;
            newState.name = state;

            auto transitions = sqf::get_variable(machine, fmt::format("{0}_transitions", state)).to_array();
            auto etransitions = sqf::get_variable(machine, fmt::format("{0}_eventtransitions", state)).to_array();

            for (auto& transitionAr : transitions) {
                auto transition = transitionAr.to_array();
                Transition newTransition;
                newTransition.name = transition[0];
                newTransition.condition = transition[1];
                newTransition.targetState = transition[2];
                newTransition.onTransitionCode = transition[3];
                codeAddTransitionHook(transition[3]);
                newState.transitions.emplace_back(std::move(newTransition));
            }

            for (auto& transitionAr : etransitions) {
                auto transition = transitionAr.to_array();
                EventTransition newTransition;
                newTransition.name = transition[0];
                auto events = transition[1].to_array();
                newTransition.events = auto_array<r_string>(events.begin(), events.end());
                newTransition.condition = transition[2];
                newTransition.targetState = transition[3];
                newTransition.onTransitionCode = transition[4];
                codeAddTransitionHook(transition[4]);
                newState.eventTransitions.emplace_back(std::move(newTransition));
            }
            newMachine.states.insert(std::move(newState));
        }

        auto getStateByName = [&newMachine](std::string_view name) -> State* {
            auto& entry = newMachine.states.get(name);
            if (map_string_to_class<State, auto_array<State>>::is_null(entry)) return {};
            return &entry;
        };

        //build state inputs
        for (auto& state : newMachine.states) {
            for (auto& trans : state.transitions) {
                auto targetState = getStateByName(trans.targetState);
                if (!targetState) continue;
                targetState->inputs.emplace_back(state.name, trans.name);
            }

            for (auto& trans : state.eventTransitions) {
                auto targetState = getStateByName(trans.targetState);
                if (!targetState) continue;
                targetState->inputs.emplace_back(state.name, trans.name);
            }
        }

        statemachines.emplace_back(std::move(newMachine));
    }


}


void intercept::on_frame() {
    {
        std::lock_guard guard(stateMachineMutex);
        for (auto& machine : statemachines) {
            auto units = sqf::get_variable(machine.machine, "cba_statemachine_list"sv).to_array(); //#TODO needs to be updated every frame
            machine.units.resize(units.size());
            int idx = 0;
            for (object unit : units) {
                auto& curUnit = machine.units[idx++];
                curUnit.unit = unit;
                r_string varname = fmt::format("cba_statemachine_state{0}", machine.id);
                curUnit.state = sqf::get_variable(unit, varname);
                curUnit.name = sqf::name(unit);
            }
        }
    }

    if (sqf::get_variable(sqf::mission_namespace(), "cba_statemachine_stateMachines"sv).to_array().size() != statemachines.size()) {
        refreshStatemachines();
    }
}

void intercept::post_init() {
    refreshStatemachines();

    launchUI();
}



/*

cba_statemachine_stateMachines array of locations

variables on that:

cba_statemachine_list - list of objects in machine - [bis_o1]
cba_statemachine_states - list of states - ["Default","Injured","Unconscious","FatalInjury","CardiacArrest","Dead"]
cba_statemachine_nextuniquestateid - next id for new state, doesn't matter - 0
cba_statemachine_updatecode - code to update list - {call ace_common_fnc_getLocalUnits}
cba_statemachine_id
cba_statemachine_initialstate - starting state - "Default"
cba_statemachine_tick - current unit in _list that is being processed


<state>_onstateleaving - leave event
<state>_onstate - enter event
<state>_onstateentered - event

<state>_transitions
array of [name of transition, condition, target state, ontransition event]
[["DeathAI",{!ace_medical_statemachine_AIUnconsciousness&& {!isPlayer _this}},"Dead",{}]]

<state>_eventtransitions
array of [name of transition, events, condition, target state, ontransition event]



*/