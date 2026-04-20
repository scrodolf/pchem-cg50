/* ==========================================================================
 * input.h — Keyboard event handling abstraction
 * ==========================================================================
 * Wraps gint's getkey() / pollevent() into a clean interface.
 *
 * gint's keyboard model:
 *   - getkey() blocks until a key is pressed and returns a key_event_t
 *   - pollevent() is non-blocking (useful for animations, not needed yet)
 *   - Key repeat is configurable via getkey_repeat()
 *
 * We configure comfortable repeat timers for scrolling through long menus.
 * ========================================================================== */

#ifndef INPUT_H
#define INPUT_H

#include <gint/keyboard.h>

/* -----------------------------------------------------------------------
 * Initialize key repeat timers.
 * first_ms:  delay before repeat starts (e.g., 400 ms)
 * repeat_ms: interval between repeats (e.g., 80 ms for smooth scrolling)
 * ----------------------------------------------------------------------- */
void input_init(int first_ms, int repeat_ms);

/* -----------------------------------------------------------------------
 * Block until a key event occurs.  Wraps getkey().
 * Returns the key_event_t with .key set to the KEYCODE_* constant.
 * ----------------------------------------------------------------------- */
key_event_t input_wait(void);

/* -----------------------------------------------------------------------
 * Check if a specific key is currently held down (non-blocking).
 * Useful for modifier detection in future features.
 * ----------------------------------------------------------------------- */
int input_key_held(int keycode);

#endif /* INPUT_H */
