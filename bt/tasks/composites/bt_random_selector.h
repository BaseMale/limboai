/**
 * bt_random_selector.h
 * =============================================================================
 * Copyright 2021-2023 Serhii Snitsaruk
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef BT_RANDOM_SELECTOR_H
#define BT_RANDOM_SELECTOR_H

#include "../bt_composite.h"

class BTRandomSelector : public BTComposite {
	GDCLASS(BTRandomSelector, BTComposite);
	TASK_CATEGORY(Composites);

private:
	int last_running_idx = 0;
	Array indicies;

protected:
	virtual void _enter() override;
	virtual Status _tick(double p_delta) override;
};

#endif // BT_RANDOM_SELECTOR_H
