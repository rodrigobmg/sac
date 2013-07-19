#include "StateMachine.h"
#include "Profiler.h"

template<typename T>
StateMachine<T>::StateMachine() : override(false), transitionning(false) {

}


template<typename T>
StateMachine<T>::StateMachine(T initState) : currentState(initState), override(false), transitionning(false) {

}

template<typename T>
StateMachine<T>::~StateMachine() {
    for(auto it=state2handler.begin(); it!=state2handler.end(); ++it) {
        delete it->second;
    }
}

template<typename T>
void StateMachine<T>::setup(T initState) {
    for(auto it=state2handler.begin(); it!=state2handler.end(); ++it) {
        it->second->setup();
    }

    currentState = (T) -1;
    transitionning = transition.readyExit = transition.dumbFrom = true;
    transition.toState = initState;
    transition.readyEnter = false;

    state2handler[initState]->onPreEnter(initState);
}

template<typename T>
void StateMachine<T>::registerState(T id, StateHandler<T>* hdl, const std::string&
    #if SAC_ENABLE_LOG || SAC_ENABLE_PROFILING
    stateDebugName
    #else
    #endif
    ) {
    LOGW_IF(state2handler.find(id) != state2handler.end(), "State id #" << id << " already registered");
	state2handler.insert(std::make_pair(id, hdl));
    #if SAC_ENABLE_LOG || SAC_ENABLE_PROFILING
    state2Name.insert(std::make_pair(id, stateDebugName));
    #endif
}

template<typename T>
void StateMachine<T>::transitionTo(T oldState, T newState) {
    LOGE_IF(state2Name.find(newState) == state2Name.end(), "No state handler defined for state: " << newState);
    LOGV(2, "Transition begins: " << state2Name[oldState] << " -> " << state2Name[newState]);
    // init transition
    PROFILE("MachineStateTransitionStart", state2Name[oldState] + "->" + state2Name[newState], InstantEvent);
    transition.fromState = oldState;
    transition.toState = newState;
    transition.readyExit = transition.readyEnter = false;
    state2handler[oldState]->onPreExit(newState);
    state2handler[newState]->onPreEnter(oldState);
    LOGV(2, "Transition preExit/Enter done");

    transitionning = true;
}

template<typename T>
void StateMachine<T>::update(float dt) {
	// Override next state if requested
	if (override) {
        if (overrideNextState == currentState)
            LOGW("overrideNextState == currentState == " << currentState);
        transitionTo(currentState, overrideNextState);
        override = false;
    }

    // Update active state
    if (!transitionning) {
    	LOGF_IF(state2handler.find(currentState) == state2handler.end(), "Current state #" << currentState << " has no handler");

        // Update state
        PROFILE("MachineStateUpdate", state2Name[currentState], BeginEvent);
        T newState = state2handler[currentState]->update(dt);
        PROFILE("MachineStateUpdate", state2Name[currentState], EndEvent);

        // New state requested ?
        if (newState != currentState) {
            transitionTo(currentState, newState);
        }
    } else {
    	if (!transition.readyExit)
    		transition.readyExit = state2handler[transition.fromState]->updatePreExit(transition.toState, dt);
    	if (!transition.readyEnter)
    		transition.readyEnter = state2handler[transition.toState]->updatePreEnter(transition.fromState, dt);

    	// If both states are ready, change state
    	if (transition.readyExit && transition.readyEnter) {
            LOGV(2, "Transition complete. New state: " << state2Name[transition.toState]);
            PROFILE("MachineStateTransitionEnd", state2Name[transition.fromState] + "->" + state2Name[transition.toState], InstantEvent);
            changeState(transition.fromState, transition.toState, transition.dumbFrom);
            transitionning = transition.dumbFrom = false;
    	}
    }
}

template<typename T>
void StateMachine<T>::forceNewState(T state) {
    LOGV(1, "Force new state: " << state);
    overrideNextState = state;
    override = true;
}

template<typename T>
T StateMachine<T>::getCurrentState() const {
    return currentState;
}

template<typename T>
void StateMachine<T>::changeState(T oldState, T newState, bool ignoreFromState) {
    if (!ignoreFromState && oldState == newState) {
        LOGW("Cannot change state : old state = new state ( = " << oldState << ")");
        return;
    }
    if (!ignoreFromState)
        state2handler[oldState]->onExit(newState);
    state2handler[newState]->onEnter(oldState);
    currentState = newState;
}

template<typename T>
void StateMachine<T>::reEnterCurrentState() {
    state2handler[currentState]->onPreEnter(currentState);
    state2handler[currentState]->onEnter(currentState);
}

template<typename T>
const std::map<T, StateHandler<T>*>& StateMachine<T>::getHandlers() const {
    return state2handler;
}

template<typename T>
void StateMachine<T>::unregisterAllStates() {
    LOGT("To be checked!!! (maybe shouldn't do that)");
    state2handler.clear();
    #if SAC_ENABLE_LOG || SAC_ENABLE_PROFILING
        state2Name.clear();
    #endif
}
