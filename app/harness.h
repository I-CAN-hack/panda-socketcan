/* SPDX-License-Identifier: Apache-2.0 */
#ifndef PANDA_HARNESS_H_
#define PANDA_HARNESS_H_

#include <stdbool.h>

int panda_harness_init(void);
bool panda_harness_flipped(void);
bool panda_harness_disconnected(void);

#endif
