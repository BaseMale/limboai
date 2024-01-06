/**
 * bb_bool.h
 * =============================================================================
 * Copyright 2021-2023 Serhii Snitsaruk
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef BB_BOOL_H
#define BB_BOOL_H

#include "bb_param.h"

class BBBool : public BBParam {
	GDCLASS(BBBool, BBParam);

protected:
	virtual Variant::Type get_type() const override { return Variant::BOOL; }
};

#endif // BB_BOOL_H