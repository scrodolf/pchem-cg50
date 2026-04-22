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
 *
 * This menu struct is generic and now serves TWO purposes:
 *   1. The main menu (topics + Navigation)
 *   2. Per-topic submenus (Descriptions / Equations / Keywords)
 *
 * The `topic_id` field in MenuItem is overloaded:
 *   - In the main menu: carries a TopicID (or TOPIC_NAVIGATION_MARKER)
 *   - In a submenu:     carries a SubtopicID (cast to int)
 *
 * Main.c is responsible for interpreting the selection correctly given
 * the current state.
 * ========================================================================== */

#ifndef MENU_H
#define MENU_H

#include "pchem.h"

/* -----------------------------------------------------------------------
 * MenuItem - one row in a scrolling menu
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *label;      /* Display text (UTF-8, may contain multi-byte) */
    int         topic_id;   /* TopicID or SubtopicID depending on context   */
} MenuItem;

/* -----------------------------------------------------------------------
 * Menu - full state for a scrolling menu screen
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *title;
    const MenuItem *items;
    int  num_items;
    int  sel;
    int  scroll_top;
} Menu;

/* -----------------------------------------------------------------------
 * MenuAction - what the caller should do after a keypress
 * ----------------------------------------------------------------------- */
typedef enum {
    MENU_ACTION_NONE,
    MENU_ACTION_SELECT,
    MENU_ACTION_BACK,
    MENU_ACTION_EXIT_APP
} MenuAction;

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
void menu_init(Menu *m, const char *title, const MenuItem *items, int count);
void menu_draw(const Menu *m);
MenuAction menu_handle_key(Menu *m, key_event_t ev);

#endif /* MENU_H */
