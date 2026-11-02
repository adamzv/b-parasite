#ifndef _PRST_ZB_RESTART_HANDLER_H_
#define _PRST_ZB_RESTART_HANDLER_H_

// Thread context only; stop serializes with an in-flight recovery request.
void prst_restart_watchdog_start(void);
void prst_restart_watchdog_stop(void);

#endif  // _PRST_ZB_RESTART_HANDLER_H_
