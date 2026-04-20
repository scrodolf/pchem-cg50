/* ==========================================================================
 * input.c — Keyboard event handling abstraction
 * ==========================================================================
 * Wraps gint's getkey() with comfortable repeat timers for scrolling.
 *
 * gint keyboard model:
 *   - getkey() is a blocking call that returns when a key is pressed
 *   - Key repeat is configured via getkey_repeat()
 *   - The returned key_event_t has:
 *       .type = KEYEV_DOWN | KEYEV_UP | KEYEV_HOLD
 *       .key  = KEY_UP, KEY_DOWN, KEY_EXE, KEY_EXIT, KEY_MENU, etc.
 *
 * We set the repeat timers to feel responsive when scrolling through
 * menus but not so fast that items blur past.
 *
 * NOTE ON API CHOICE:
 *   gint 2.9+ deprecates getkey_repeat() in favor of
 *   keydev_set_standard_repeats(), but the latter is declared in
 *   <gint/drivers/keydev.h> which may not exist in all gint versions.
 *   We use getkey_repeat() for maximum compatibility (it still works
 *   correctly) and suppress the deprecation warning with a pragma.
 * ========================================================================== */

#include "input.h"
#include <gint/keyboard.h>

/* -----------------------------------------------------------------------
 * input_init — configure key repeat timing
 *
 * first_ms:  milliseconds before the first repeat fires (e.g., 400)
 * repeat_ms: milliseconds between subsequent repeats (e.g., 80)
 *
 * These values were tuned empirically:
 *   - 400 ms initial delay prevents accidental double-taps
 *   - 80 ms repeat gives smooth scrolling at ~12 items/second
 *
 * getkey_repeat() takes times in units of 1/128 seconds (ticks).
 * We convert from milliseconds:  ticks = ms * 128 / 1000
 * ----------------------------------------------------------------------- */
void input_init(int first_ms, int repeat_ms)
{
    /* Convert milliseconds to 1/128-second ticks for getkey_repeat() */
    int first_ticks  = (first_ms  * 128 + 500) / 1000;
    int repeat_ticks = (repeat_ms * 128 + 500) / 1000;

    /* Suppress the deprecation warning for getkey_repeat().
     * The replacement (keydev_set_standard_repeats) is in a header
     * that doesn't exist in all gint versions.  getkey_repeat()
     * works correctly — it's just flagged for future removal. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    getkey_repeat(first_ticks, repeat_ticks);
#pragma GCC diagnostic pop
}

/* -----------------------------------------------------------------------
 * input_wait — block until a key event and return it
 *
 * This is a thin wrapper around getkey() that could be extended later
 * to filter events, implement debouncing, or add timeout behavior.
 * ----------------------------------------------------------------------- */
key_event_t input_wait(void)
{
    return getkey();
}

/* -----------------------------------------------------------------------
 * input_key_held — check if a key is currently pressed (non-blocking)
 *
 * Uses gint's keydown() which reads the current keyboard matrix state.
 * Useful for future features like "hold SHIFT to page-scroll" or
 * detecting modifier combos.
 * ----------------------------------------------------------------------- */
int input_key_held(int keycode)
{
    return keydown(keycode);
}
