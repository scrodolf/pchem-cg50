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
 *        +-[Navigation EXE]--> NAVIGATION   -[EXIT]-> MAIN_MENU
 *        +-[Formula EXE]-----> FORMULA       -[EXIT]-> MAIN_MENU
 *        +-[Greek EXE]-------> GREEKSYMBOLS  -[EXIT]-> MAIN_MENU
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
 *   7. Statistical Mechanics
 *   * Navigation (All Terms)
 *   * Formula Guide
 *   * Greek Symbols
 *
 * Sentinel values (TOPIC_NAVIGATION_MARKER = 100, TOPIC_FORMULA_MARKER = 101,
 * TOPIC_GREEKSYMBOLS_MARKER = 102) distinguish the three utility tabs from
 * the seven topic entries in the selection dispatcher.
 * ========================================================================== */

#include <gint/display.h>
#include <gint/keyboard.h>

#include "pchem.h"
#include "menu.h"
#include "topics.h"
#include "navigation.h"
#include "formula.h"
#include "greeksymbols.h"
#include "input.h"
#include "render.h"

/* -----------------------------------------------------------------------
 * Main menu entries: 7 topics + 3 utility tabs.
 * ----------------------------------------------------------------------- */
static const MenuItem main_menu_items[NUM_MAIN_MENU_ENTRIES] = {
    { "1. Particle in a Box",     (int)TOPIC_PIB                  },
    { "2. Commutators & Spin",    (int)TOPIC_COMMUTATORS           },
    { "3. Harmonic Osc. & Rotor", (int)TOPIC_OSCILLATOR            },
    { "4. Diatomic Spectroscopy", (int)TOPIC_SPECTROSCOPY          },
    { "5. Hydrogen Atom",         (int)TOPIC_HYDROGEN              },
    { "6. Many-Electron Atoms",   (int)TOPIC_MULTIELECTRON         },
    { "7. Statistical Mechanics", (int)TOPIC_STATMECH              },
    { "* Navigation (All Terms)", (int)TOPIC_NAVIGATION_MARKER     },
    { "* Formula Guide",          (int)TOPIC_FORMULA_MARKER        },
    { "* Greek Symbols",          (int)TOPIC_GREEKSYMBOLS_MARKER   },
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
    FormulaScreen    formula_screen;
    GreekScreen      greek_screen;

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
        case STATE_FORMULA:
            formula_draw(&formula_screen);
            break;
        case STATE_GREEKSYMBOLS:
            greek_draw(&greek_screen);
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
                    navigation_init(&nav_screen);
                    state = STATE_NAVIGATION;
                } else if (sel_id == (int)TOPIC_FORMULA_MARKER) {
                    formula_init(&formula_screen);
                    state = STATE_FORMULA;
                } else if (sel_id == (int)TOPIC_GREEKSYMBOLS_MARKER) {
                    greek_init(&greek_screen);
                    state = STATE_GREEKSYMBOLS;
                } else {
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
                topic_init(&topic_screen,
                           submenu.topic,
                           (SubtopicID)submenu.sel);
                state = STATE_TOPIC_VIEW;
                break;
            case 2:
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
            if      (r == 1) state = STATE_SUBMENU;
            else if (r == 2) state = STATE_EXIT;
            break;
        }

        /* ~~~~~~~~~~~~~~~ NAVIGATION SCREEN ~~~~~~~~~~~~~~~ */
        case STATE_NAVIGATION: {
            int r = navigation_handle_key(&nav_screen, ev);
            if      (r == 1) state = STATE_MAIN_MENU;
            else if (r == 2) state = STATE_EXIT;
            break;
        }

        /* ~~~~~~~~~~~~~~~ FORMULA GUIDE SCREEN ~~~~~~~~~~~~~~~ */
        case STATE_FORMULA: {
            int r = formula_handle_key(&formula_screen, ev);
            if      (r == 1) state = STATE_MAIN_MENU;
            else if (r == 2) state = STATE_EXIT;
            break;
        }

        /* ~~~~~~~~~~~~~~~ GREEK SYMBOLS SCREEN ~~~~~~~~~~~~~~~ */
        case STATE_GREEKSYMBOLS: {
            int r = greek_handle_key(&greek_screen, ev);
            if      (r == 1) state = STATE_MAIN_MENU;
            else if (r == 2) state = STATE_EXIT;
            break;
        }

        default:
            break;
        }
    }

    return 0;
}
