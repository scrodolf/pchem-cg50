/* ==========================================================================
 * main.c - Entry point and top-level state machine
 * ==========================================================================
 *
 * ARCHITECTURE
 * ============
 *    MAIN_MENU -[topic EXE]-> SUBMENU -[subtopic EXE]-> TOPIC_VIEW
 *        |                       |                         |
 *        |                       +--[EXIT]-----------------+
 *        |                                           <-- to SUBMENU
 *        +-[Navigation EXE]-> NAVIGATION -[EXIT]-> MAIN_MENU
 *
 *    MENU  always exits the add-in.
 *    EXIT  always goes back one level.
 *    EXE   selects / opens.
 *
 * MAIN MENU LAYOUT
 * ================
 *   1. Particle in a Box
 *   2. Commutators & Spin
 *   3. Harmonic Osc. & Rotor
 *   4. Diatomic Spectroscopy
 *   5. Hydrogen Atom
 *   6. Many-Electron Atoms
 *   * Navigation                  <-- new: global keyword/symbol navigator
 *
 * The Navigation entry is tagged with TOPIC_NAVIGATION_MARKER so the
 * selection dispatcher can distinguish it from a topic selection.
 * ========================================================================== */

#include <gint/display.h>
#include <gint/keyboard.h>

#include "pchem.h"
#include "menu.h"
#include "topics.h"
#include "navigation.h"
#include "input.h"
#include "render.h"

/* -----------------------------------------------------------------------
 * Main menu entries: 6 topics + 1 Navigation entry.
 * ----------------------------------------------------------------------- */
static const MenuItem main_menu_items[NUM_MAIN_MENU_ENTRIES] = {
    { "1. Particle in a Box",     (int)TOPIC_PIB           },
    { "2. Commutators & Spin",    (int)TOPIC_COMMUTATORS   },
    { "3. Harmonic Osc. & Rotor", (int)TOPIC_OSCILLATOR    },
    { "4. Diatomic Spectroscopy", (int)TOPIC_SPECTROSCOPY  },
    { "5. Hydrogen Atom",         (int)TOPIC_HYDROGEN      },
    { "6. Many-Electron Atoms",   (int)TOPIC_MULTIELECTRON },
    { "* Navigation (All Terms)", (int)TOPIC_NAVIGATION_MARKER },
};

/* -----------------------------------------------------------------------
 * main() - gint entry point
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* Subsystem init */
    input_init(400, 80);        /* key repeat */
    sym_table_init();           /* symbol table */

    /* Long-lived screen state (kept across transitions) */
    Menu             main_menu;
    SubMenuScreen    submenu;
    TopicScreen      topic_screen;
    NavigationScreen nav_screen;

    menu_init(&main_menu, "Physical Chemistry",
              main_menu_items, NUM_MAIN_MENU_ENTRIES);

    AppState state = STATE_MAIN_MENU;

    /* ==================================================================
     * MAIN EVENT LOOP
     * ================================================================== */
    while (state != STATE_EXIT) {

        /* ---- Draw the current screen ---- */
        dclear(COL_BG);

        switch (state) {
        case STATE_MAIN_MENU:
            menu_draw(&main_menu);
            break;
        case STATE_SUBMENU:
            submenu_draw(&submenu);
            break;
        case STATE_TOPIC_VIEW:
            topic_draw(&topic_screen);
            break;
        case STATE_NAVIGATION:
            navigation_draw(&nav_screen);
            break;
        default:
            break;
        }

        dupdate();

        /* ---- Wait for a key event ---- */
        key_event_t ev = input_wait();

        /* ---- Dispatch on current state ---- */
        switch (state) {

        /* ~~~~~~~~~~~~~~~ MAIN MENU ~~~~~~~~~~~~~~~ */
        case STATE_MAIN_MENU: {
            MenuAction act = menu_handle_key(&main_menu, ev);
            switch (act) {
            case MENU_ACTION_SELECT: {
                int sel_id = main_menu_items[main_menu.sel].topic_id;

                if (sel_id == (int)TOPIC_NAVIGATION_MARKER) {
                    /* Enter the global keyword/symbol navigator */
                    navigation_init(&nav_screen);
                    state = STATE_NAVIGATION;
                } else {
                    /* Enter the topic's submenu */
                    submenu_init(&submenu, (TopicID)sel_id);
                    state = STATE_SUBMENU;
                }
                break;
            }
            case MENU_ACTION_EXIT_APP:
            case MENU_ACTION_BACK:
                state = STATE_EXIT;
                break;
            default:
                break;
            }
            break;
        }

        /* ~~~~~~~~~~~~~~~ SUBMENU ~~~~~~~~~~~~~~~ */
        case STATE_SUBMENU: {
            int r = submenu_handle_key(&submenu, ev);
            switch (r) {
            case 1:
                /* Subtopic selected: open the content view */
                topic_init(&topic_screen,
                           submenu.topic,
                           (SubtopicID)submenu.sel);
                state = STATE_TOPIC_VIEW;
                break;
            case 2:
                /* EXIT -> back to main menu */
                state = STATE_MAIN_MENU;
                break;
            case 3:
                state = STATE_EXIT;
                break;
            default:
                break;
            }
            break;
        }

        /* ~~~~~~~~~~~~~~~ TOPIC CONTENT VIEW ~~~~~~~~~~~~~~~ */
        case STATE_TOPIC_VIEW: {
            int r = topic_handle_key(&topic_screen, ev);
            if (r == 1) {
                /* EXIT -> back to submenu */
                state = STATE_SUBMENU;
            } else if (r == 2) {
                state = STATE_EXIT;
            }
            break;
        }

        /* ~~~~~~~~~~~~~~~ NAVIGATION SCREEN ~~~~~~~~~~~~~~~ */
        case STATE_NAVIGATION: {
            int r = navigation_handle_key(&nav_screen, ev);
            if (r == 1) {
                state = STATE_MAIN_MENU;
            } else if (r == 2) {
                state = STATE_EXIT;
            }
            break;
        }

        default:
            break;
        }
    }

    return 0;
}
