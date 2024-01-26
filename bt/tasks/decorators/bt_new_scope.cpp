/**
 * bt_new_scope.cpp
 * =============================================================================
 * Copyright 2021-2023 Serhii Snitsaruk
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "bt_new_scope.h"

void BTNewScope::initialize(Node *p_agent, const Ref<Blackboard> &p_blackboard) {
	ERR_FAIL_COND(p_agent == nullptr);
	ERR_FAIL_COND(p_blackboard == nullptr);

	Ref<Blackboard> bb;
	if (blackboard_plan.is_valid()) {
		bb = blackboard_plan->create_blackboard();
	} else {
		bb = Ref<Blackboard>(memnew(Blackboard));
	}

	bb->set_parent(p_blackboard);

	BTDecorator::initialize(p_agent, bb);
}

BT::Status BTNewScope::_tick(double p_delta) {
	ERR_FAIL_COND_V_MSG(get_child_count() == 0, FAILURE, "BT decorator has no child.");
	return get_child(0)->execute(p_delta);
}

void BTNewScope::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_blackboard_plan", "p_plan"), &BTNewScope::set_blackboard_plan);
	ClassDB::bind_method(D_METHOD("get_blackboard_plan"), &BTNewScope::get_blackboard_plan);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "blackboard_plan", PROPERTY_HINT_RESOURCE_TYPE, "BlackboardPlan", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_blackboard_plan", "get_blackboard_plan");
}
