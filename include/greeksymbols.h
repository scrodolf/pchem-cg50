/* ==========================================================================
 * greeksymbols.h — Greek Symbols Dictionary tab
 * ==========================================================================
 *
 * PURPOSE
 * -------
 * A scrollable reference screen that links every Greek glyph used in the
 * add-in to its spelled-out name, ASCII surrogate character, and meaning(s)
 * with subject-area context.
 *
 * VISUAL LAYOUT PER ENTRY
 * -----------------------
 *   [glyph]  [name]   ->  sub: '[x]'
 *            [Meaning 1]
 *            [Context 1]           ← indented, muted colour
 *            [Meaning 2]           ← only when a 2nd meaning exists
 *            [Context 2]
 *   ─────────────────────────────────────────────────────────────────
 *
 * GLYPH / SURROGATE
 * -----------------
 * In the v6 ASCII-surrogate system the "glyph" rendered on screen IS the
 * surrogate character (e.g. psi → 'y', phi → 'f').  Both columns therefore
 * show the same ASCII character; the dictionary makes this mapping explicit
 * so students understand which letter to look for in equations.
 * ========================================================================== */

#ifndef GREEKSYMBOLS_H
#define GREEKSYMBOLS_H

#include "pchem.h"

typedef struct {
    int scroll_y;       /* current scroll offset in pixels                  */
    int content_h;      /* total rendered content height (set on draw)      */

    /* v5 long-press tracking */
    int held_key;
    int held_count;
    int jumped;
} GreekScreen;

void greek_init(GreekScreen *gs);
void greek_draw(GreekScreen *gs);   /* mutates gs->content_h */

/* Returns:
 *   0 = consumed (scroll / no-op)
 *   1 = EXIT → return to MAIN_MENU
 *   2 = MENU → exit the add-in      */
int  greek_handle_key(GreekScreen *gs, key_event_t ev);

#endif /* GREEKSYMBOLS_H */
