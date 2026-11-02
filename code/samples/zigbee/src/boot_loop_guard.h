#ifndef _PRST_ZB_BOOT_LOOP_GUARD_H_
#define _PRST_ZB_BOOT_LOOP_GUARD_H_

#include <stdbool.h>
#include <stdint.h>

// Returns the confirmed crash count before this boot; other resets clear the streak.
// Uses crash-only storage, leaving legacy all-boot counts untouched. FS errors return zero.
uint32_t prst_boot_loop_guard_register_boot(bool crash_reset);

// Clears the count off the Zigbee thread; a failed clear can be requested again.
void prst_boot_loop_guard_clear_async();

#endif  // _PRST_ZB_BOOT_LOOP_GUARD_H_
