/* ==========================================================================
 * formula.h — Formula Guide tab: all equations from all topics
 * ==========================================================================
 *
 * PURPOSE
 * -------
 * Displays every EquationEntry from every topic in a single, scrollable
 * screen.  Equations are rendered as live MathNode ASTs (via the same
 * builder functions used inside the topic content views) and are
 * accompanied by their variable-description text.
 *
 * LAYOUT PER TOPIC SECTION
 * ------------------------
 *   ┌─ Topic Title (accent bar) ────────────────────────────────────────┐
 *   │  Equation label                                                   │
 *   │    [rendered math expression — wraps if wider than screen]        │
 *   │  ─────────────────────────────────────────── (thin divider)       │
 *   │    var1 : description …                                           │
 *   │    var2 : description …                                           │
 *   │  ═══════════════════════════════════════════ (entry separator)    │
 *   │  Equation label                                                   │
 *   │    …                                                              │
 *   └───────────────────────────────────────────────────────────────────┘
 *
 * POOL MANAGEMENT
 * ---------------
 * render_pool_reset() is called before every individual equation so the
 * 512-node pool is never exhausted across the full cross-topic render.
 * Drawing is complete before the next reset, so no stale pointers arise.
 * ========================================================================== */

#ifndef FORMULA_H
#define FORMULA_H

#include "pchem.h"

/* Runtime state — identical shape to NavigationScreen for consistency. */
typedef struct {
    int scroll_y;       /* current scroll offset in pixels                  */
    int content_h;      /* total rendered content height (set on draw)      */

    /* v5 long-press tracking (same rules as all other scrollable screens) */
    int held_key;
    int held_count;
    int jumped;
} FormulaScreen;

/* Initialise (or reset) a FormulaScreen to top-of-content state. */
void formula_init(FormulaScreen *fs);

/* Render the full Formula Guide to VRAM.  Mutates fs->content_h.
 * Caller must call dclear() before and dupdate() after. */
void formula_draw(FormulaScreen *fs);

/* Process one keyboard event.
 * Returns:
 *   0 = consumed (scroll / no-op)
 *   1 = EXIT key → caller should return to MAIN_MENU
 *   2 = MENU key → caller should exit the add-in              */
int  formula_handle_key(FormulaScreen *fs, key_event_t ev);

#endif /* FORMULA_H */
