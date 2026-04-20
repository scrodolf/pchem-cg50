/* ==========================================================================
 * main.c — Entry point and top-level state machine
 * ==========================================================================
 *
 * APPLICATION ARCHITECTURE
 * ========================
 * The add-in is a flat state machine with three states:
 *
 *    ┌─────────────┐   EXE    ┌──────────────┐
 *    │  MAIN_MENU  │────────►│  TOPIC_VIEW  │
 *    │  (scrolling │◄────────│  (content +  │
 *    │   list)     │  EXIT   │   equations) │
 *    └─────┬───────┘         └──────────────┘
 *          │ MENU
 *          ▼
 *      (exit app)
 *
 * The main loop:
 *   1. Draws the current screen to VRAM
 *   2. Calls dupdate() to commit the framebuffer
 *   3. Blocks on input_wait() for a key event
 *   4. Dispatches the event to the active screen handler
 *   5. Updates state if the handler requests a transition
 *
 * KEY BINDING CONTRACT (from requirements):
 *   EXIT  = go back one menu level (always)
 *   MENU  = exit the add-in entirely (from any screen)
 *   EXE   = select / enter
 *   UP/DN = scroll within the current screen
 *
 * FRAMEBUFFER PROTOCOL (gint):
 *   All drawing goes to an off-screen VRAM buffer.  Nothing appears on the
 *   LCD until dupdate() is called.  We call dupdate() exactly once per loop
 *   iteration, after all drawing for the frame is complete.  This prevents
 *   flickering and partial draws.
 * ========================================================================== */

#include <gint/display.h>
#include <gint/keyboard.h>
#include <string.h>

#include "pchem.h"
#include "menu.h"
#include "topics.h"
#include "input.h"
#include "render.h"

/* -----------------------------------------------------------------------
 * Main Menu Definition
 * -----------------------------------------------------------------------
 * These six topics come directly from the lecture slides (PDF pages 1–4):
 *   Lectures 5, 6, 7–8, 8, (9 implied), and 10.
 * The labels are kept short enough to fit in one menu row at 18px font.
 * ----------------------------------------------------------------------- */
static const MenuItem main_menu_items[NUM_TOPICS] = {
    { "1. Particle in a Box",          TOPIC_PIB           },
    { "2. Commutators & Spin",         TOPIC_COMMUTATORS   },
    { "3. Harmonic Osc. & Rotor",      TOPIC_OSCILLATOR    },
    { "4. Diatomic Spectroscopy",      TOPIC_SPECTROSCOPY  },
    { "5. Hydrogen Atom",              TOPIC_HYDROGEN      },
    { "6. Many-Electron Atoms",        TOPIC_MULTIELECTRON },
};

/* -----------------------------------------------------------------------
 * main() — gint entry point
 * -----------------------------------------------------------------------
 * gint add-ins use a normal main().  The gint runtime handles hardware
 * init, VRAM allocation, and OS-level hooks before calling us.
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* -- Initialize subsystems -- */
    input_init(400, 80);        /* Key repeat: 400 ms initial, 80 ms rate  */
    sym_table_init();           /* Load OS multi-byte symbol lookup table   */

    /* -- Set up the main menu -- */
    Menu main_menu;
    menu_init(&main_menu, "Physical Chemistry", main_menu_items, NUM_TOPICS);

    /* -- Topic screen (reused across selections) -- */
    TopicScreen topic_screen;

    /* -- State machine -- */
    AppState state = STATE_MAIN_MENU;

    /* ==================================================================
     * MAIN EVENT LOOP
     * ==================================================================
     * We loop until state == STATE_EXIT.  Each iteration:
     *   1. Clear VRAM
     *   2. Draw the active screen
     *   3. Commit framebuffer (dupdate)
     *   4. Wait for key input
     *   5. Dispatch to the active handler and update state
     * ================================================================== */
    while (state != STATE_EXIT) {

        /* ---- Step 1–2: Draw the current screen ---- */
        dclear(COL_BG);

        switch (state) {
        case STATE_MAIN_MENU:
            menu_draw(&main_menu);
            break;

        case STATE_TOPIC_VIEW:
            topic_draw(&topic_screen);
            break;

        default:
            /* Future states (subtopic, etc.) — draw nothing extra */
            break;
        }

        /* ---- Step 3: Commit framebuffer to LCD ---- */
        dupdate();

        /* ---- Step 4: Wait for key event (blocking) ---- */
        key_event_t ev = input_wait();

        /* ---- Step 5: Dispatch to current screen's handler ---- */
        switch (state) {

        /* ~~~~~~~~~~~~~~~ MAIN MENU STATE ~~~~~~~~~~~~~~~ */
        case STATE_MAIN_MENU: {
            MenuAction act = menu_handle_key(&main_menu, ev);

            switch (act) {
            case MENU_ACTION_SELECT:
                /* Transition: MAIN_MENU → TOPIC_VIEW
                 * The selected topic ID is stored in the menu item. */
                topic_init(&topic_screen,
                           main_menu_items[main_menu.sel].topic_id);
                render_pool_reset();   /* Fresh node pool for this topic */
                state = STATE_TOPIC_VIEW;
                break;

            case MENU_ACTION_EXIT_APP:
                /* MENU key pressed — exit the entire add-in */
                state = STATE_EXIT;
                break;

            case MENU_ACTION_BACK:
                /* EXIT from main menu also exits the add-in
                 * (there is no level above the main menu) */
                state = STATE_EXIT;
                break;

            default:
                /* MENU_ACTION_NONE — key was consumed (scroll, etc.) */
                break;
            }
            break;
        }

        /* ~~~~~~~~~~~~~~~ TOPIC VIEW STATE ~~~~~~~~~~~~~~~ */
        case STATE_TOPIC_VIEW: {
            int result = topic_handle_key(&topic_screen, ev);

            if (result == 1) {
                /* EXIT key → go back to main menu */
                state = STATE_MAIN_MENU;
            } else if (result == 2) {
                /* MENU key → exit add-in entirely */
                state = STATE_EXIT;
            }
            break;
        }

        default:
            break;
        }
    }

    /* -- Clean exit: gint handles OS restoration automatically -- */
    return 0;
}
