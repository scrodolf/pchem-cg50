/* ==========================================================================
 * menu.h — Scrolling menu system
 * ==========================================================================
 * Modeled after the "Utilities Add-in" (gbl08ma) and the "Sample Scrolling
 * Menu" (tifreak8x) from the Cemetech archive.
 *
 * Design:
 *   - A Menu holds an array of MenuItems and tracks selection + scroll state.
 *   - The menu renderer draws only the visible window of items, with a
 *     scroll indicator rail on the right edge.
 *   - Input handling is decoupled: menu_handle_key() returns an action code
 *     so the caller decides what to do (enter topic, go back, exit app).
 * ========================================================================== */

#ifndef MENU_H
#define MENU_H

#include "pchem.h"

/* -----------------------------------------------------------------------
 * MenuItem — one row in a scrolling menu
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *label;      /* Display text (UTF-8, may contain multi-byte) */
    TopicID     topic_id;   /* Which topic this item represents             */
} MenuItem;

/* -----------------------------------------------------------------------
 * Menu — full state for a scrolling menu screen
 *
 * Fields:
 *   title       — text drawn in the header bar
 *   items       — pointer to array of MenuItems
 *   num_items   — total number of items
 *   sel         — currently highlighted item index (0-based)
 *   scroll_top  — index of the first visible item (for scrolling)
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *title;
    const MenuItem *items;
    int  num_items;
    int  sel;               /* Selected index within items[] */
    int  scroll_top;        /* First visible item index      */
} Menu;

/* -----------------------------------------------------------------------
 * MenuAction — what the caller should do after a keypress
 * ----------------------------------------------------------------------- */
typedef enum {
    MENU_ACTION_NONE,       /* Key consumed internally (scroll, etc.)      */
    MENU_ACTION_SELECT,     /* User pressed EXE on the selected item       */
    MENU_ACTION_BACK,       /* User pressed EXIT — go back one level       */
    MENU_ACTION_EXIT_APP    /* User pressed MENU — exit the add-in         */
} MenuAction;

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/* Initialize a menu with the given title and item array. Resets selection
 * to index 0 and scroll to the top. */
void menu_init(Menu *m, const char *title, const MenuItem *items, int count);

/* Draw the menu to the VRAM framebuffer.  Does NOT call dupdate() — the
 * caller is responsible for the commit cycle. */
void menu_draw(const Menu *m);

/* Process a single key event.  Returns a MenuAction telling the caller
 * what high-level action to take.  If MENU_ACTION_SELECT is returned,
 * m->sel contains the chosen item index. */
MenuAction menu_handle_key(Menu *m, key_event_t ev);

#endif /* MENU_H */
