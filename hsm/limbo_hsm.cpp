/**
 * limbo_hsm.cpp
 * =============================================================================
 * Copyright 2021-2023 Serhii Snitsaruk
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "limbo_hsm.h"

#ifdef LIMBOAI_MODULE
#include "core/config/engine.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/typedefs.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"
#endif // LIMBOAI_MODULE

VARIANT_ENUM_CAST(LimboHSM::UpdateMode);

void LimboHSM::set_active(bool p_active) {
	ERR_FAIL_COND_MSG(agent == nullptr, "LimboHSM is not initialized.");
	ERR_FAIL_COND_MSG(p_active && initial_state == nullptr, "LimboHSM has no initial substate candidate.");

	if (active == p_active) {
		return;
	}

	active = p_active;
	switch (update_mode) {
		case UpdateMode::IDLE: {
			set_process(p_active);
			set_physics_process(false);
		} break;
		case UpdateMode::PHYSICS: {
			set_process(false);
			set_physics_process(p_active);
		} break;
		case UpdateMode::MANUAL: {
			set_process(false);
			set_physics_process(false);
		} break;
	}
	set_process_input(p_active);

	if (active) {
		_enter();
	} else {
		_exit();
	}
}

void LimboHSM::_change_state(LimboState *p_state) {
	ERR_FAIL_COND(p_state == nullptr);
	ERR_FAIL_COND(p_state->get_parent() != this);

	if (active_state) {
		active_state->_exit();
	}

	active_state = p_state;
	active_state->_enter();

	emit_signal(LimboStringNames::get_singleton()->state_changed, active_state);
}

void LimboHSM::_enter() {
	ERR_FAIL_COND_MSG(get_child_count() == 0, "LimboHSM has no candidate for initial substate.");
	ERR_FAIL_COND(active_state != nullptr);
	ERR_FAIL_COND_MSG(initial_state == nullptr, "LimboHSM: Initial state is not set.");

	LimboState::_enter();
	_change_state(initial_state);
}

void LimboHSM::_exit() {
	ERR_FAIL_COND(active_state == nullptr);
	active_state->_exit();
	active_state = nullptr;
	LimboState::_exit();
}

void LimboHSM::_update(double p_delta) {
	if (active) {
		ERR_FAIL_COND(active_state == nullptr);
		LimboState::_update(p_delta);
		active_state->_update(p_delta);
	}
}

void LimboHSM::update(double p_delta) {
	_update(p_delta);
}

void LimboHSM::add_transition(LimboState *p_from_state, LimboState *p_to_state, const String &p_event) {
	ERR_FAIL_COND_MSG(p_from_state != nullptr && p_from_state->get_parent() != this, "LimboHSM: Unable to add a transition from a state that is not an immediate child of mine.");
	ERR_FAIL_COND_MSG(p_to_state == nullptr, "LimboHSM: Unable to add a transition to a null state.");
	ERR_FAIL_COND_MSG(p_to_state->get_parent() != this, "LimboHSM: Unable to add a transition to a state that is not an immediate child of mine.");
	ERR_FAIL_COND_MSG(p_event.is_empty(), "LimboHSM: Failed to add transition due to empty event string.");

	uint64_t key = _get_transition_key(p_from_state, p_event);
	transitions[key] = Object::cast_to<LimboState>(p_to_state);
}

LimboState *LimboHSM::get_leaf_state() const {
	LimboHSM *hsm = const_cast<LimboHSM *>(this);
	while (hsm->active_state != nullptr && hsm->active_state->is_class("LimboHSM")) {
		hsm = Object::cast_to<LimboHSM>(hsm->active_state);
	}
	if (hsm->active_state) {
		return hsm->active_state;
	} else {
		return hsm;
	}
}

void LimboHSM::set_initial_state(LimboState *p_state) {
	ERR_FAIL_COND(p_state == nullptr || !p_state->is_class("LimboState"));
	initial_state = Object::cast_to<LimboState>(p_state);
}

bool LimboHSM::dispatch(const String &p_event, const Variant &p_cargo) {
	ERR_FAIL_COND_V(p_event.is_empty(), false);

	bool event_consumed = false;

	if (active_state) {
		event_consumed = active_state->dispatch(p_event, p_cargo);
	}

	if (!event_consumed) {
		event_consumed = LimboState::dispatch(p_event, p_cargo);
	}

	if (!event_consumed && active_state) {
		uint64_t key = _get_transition_key(active_state, p_event);
		LimboState *to_state = nullptr;
		if (transitions.has(key)) {
			to_state = transitions[key];
		}
		if (to_state == nullptr) {
			// Get ANYSTATE transition.
			key = _get_transition_key(nullptr, p_event);
			if (transitions.has(key)) {
				to_state = transitions[key];
			}
		}
		if (to_state != nullptr) {
			bool permitted = true;
			if (to_state->guard_callable.is_valid()) {
				Variant ret;

#ifdef LIMBOAI_MODULE
				Callable::CallError ce;
				to_state->guard_callable.callp(nullptr, 0, ret, ce);
				if (unlikely(ce.error != Callable::CallError::CALL_OK)) {
					ERR_PRINT_ONCE("LimboHSM: Error calling substate's guard callable: " + Variant::get_callable_error_text(to_state->guard_callable, nullptr, 0, ce));
				}
#elif LIMBOAI_GDEXTENSION
				ret = to_state->guard_callable.call();
#endif

				if (unlikely(ret.get_type() != Variant::BOOL)) {
					ERR_PRINT_ONCE(vformat("State guard callable %s returned non-boolean value (%s).", to_state->guard_callable, to_state));
				} else {
					permitted = bool(ret);
				}
			}
			if (permitted) {
				_change_state(to_state);
				event_consumed = true;
			}
		}
	}

	if (!event_consumed && p_event == LW_NAME(EVENT_FINISHED) && !(get_parent() && get_parent()->is_class("LimboState"))) {
		_exit();
	}

	return event_consumed;
}

void LimboHSM::initialize(Node *p_agent, const Ref<Blackboard> &p_parent_scope) {
	ERR_FAIL_COND(p_agent == nullptr);
	if (!p_parent_scope.is_null()) {
		blackboard->set_parent(p_parent_scope);
	}
	_initialize(p_agent, nullptr);

	if (initial_state == nullptr) {
		initial_state = Object::cast_to<LimboState>(get_child(0));
	}
}

void LimboHSM::_initialize(Node *p_agent, const Ref<Blackboard> &p_blackboard) {
	ERR_FAIL_COND(p_agent == nullptr);
	ERR_FAIL_COND_MSG(agent != nullptr, "LimboAI: HSM already initialized.");
	ERR_FAIL_COND_MSG(get_child_count() == 0, "Cannot initialize LimboHSM: no candidate for initial substate.");

	if (initial_state == nullptr) {
		initial_state = Object::cast_to<LimboState>(get_child(0));
		ERR_FAIL_COND_MSG(initial_state == nullptr, "LimboHSM: Child at index 0 is not a LimboState.");
	}

	LimboState::_initialize(p_agent, p_blackboard);

	for (int i = 0; i < get_child_count(); i++) {
		LimboState *c = Object::cast_to<LimboState>(get_child(i));
		if (unlikely(c == nullptr)) {
			ERR_PRINT(vformat("LimboHSM: Child at index %d is not a LimboState.", i));
		} else {
			c->_initialize(agent, blackboard);
		}
	}
}

void LimboHSM::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
		} break;
		case NOTIFICATION_PROCESS: {
			_update(get_process_delta_time());
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			_update(get_physics_process_delta_time());
		} break;
	}
}

void LimboHSM::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_update_mode", "p_mode"), &LimboHSM::set_update_mode);
	ClassDB::bind_method(D_METHOD("get_update_mode"), &LimboHSM::get_update_mode);

	ClassDB::bind_method(D_METHOD("set_initial_state", "p_state"), &LimboHSM::set_initial_state);
	ClassDB::bind_method(D_METHOD("get_initial_state"), &LimboHSM::get_initial_state);

	ClassDB::bind_method(D_METHOD("get_active_state"), &LimboHSM::get_active_state);
	ClassDB::bind_method(D_METHOD("get_leaf_state"), &LimboHSM::get_leaf_state);
	ClassDB::bind_method(D_METHOD("set_active", "p_active"), &LimboHSM::set_active);
	ClassDB::bind_method(D_METHOD("update", "p_delta"), &LimboHSM::update);
	ClassDB::bind_method(D_METHOD("add_transition", "p_from_state", "p_to_state", "p_event"), &LimboHSM::add_transition);
	ClassDB::bind_method(D_METHOD("anystate"), &LimboHSM::anystate);

	ClassDB::bind_method(D_METHOD("initialize", "p_agent", "p_parent_scope"), &LimboHSM::initialize, Variant());

	BIND_ENUM_CONSTANT(IDLE);
	BIND_ENUM_CONSTANT(PHYSICS);
	BIND_ENUM_CONSTANT(MANUAL);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "update_mode", PROPERTY_HINT_ENUM, "Idle, Physics, Manual"), "set_update_mode", "get_update_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ANYSTATE", PROPERTY_HINT_RESOURCE_TYPE, "LimboState", 0), "", "anystate");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "initial_state", PROPERTY_HINT_RESOURCE_TYPE, "LimboState", 0), "set_initial_state", "get_initial_state");

	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::OBJECT, "p_state", PROPERTY_HINT_RESOURCE_TYPE, "LimboState", 0)));
}

LimboHSM::LimboHSM() {
	update_mode = UpdateMode::PHYSICS;
	active_state = nullptr;
	initial_state = nullptr;
}
