#include <intercept.hpp>

class Transition {
public:
    r_string name;
    game_value condition;
    r_string targetState;
    game_value onTransitionCode;
    int ui_uid;
    bool active = false;
};

class EventTransition {
public:
    r_string name;
    auto_array<r_string> events;
    game_value condition;
    r_string targetState;
    game_value onTransitionCode;
    int ui_uid;
    bool active = false;
};

class State {
public:
    r_string name;
    std::vector<Transition> transitions;
    std::vector<EventTransition> eventTransitions;
    int ui_uid; //inputs are ui_uid + 1 + input index

    std::vector<std::pair<r_string, r_string>> inputs;

    r_string get_map_key() const {
        return name;
    }

    void renderNode(int& uniqueId, bool isActive);
};

class Unit {
public:
    game_value unit;
    r_string name;
    r_string varname;
    r_string state;
};

class Statemachine {
public:
    map_string_to_class<State, auto_array<State>> states;
    location machine;
    auto_array<Unit> units;
    int id;
};

extern std::vector<Statemachine> statemachines;
extern std::mutex stateMachineMutex;