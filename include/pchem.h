/* ==========================================================================
 * pchem.h — Global definitions for the PChem add-in
 * ==========================================================================
 * This header is included by every translation unit. It defines:
 *   - Screen geometry constants (fx-CG50 = 396×224 pixels)
 *   - The top-level application state machine enum
 *   - Forward declarations shared across modules
 * ========================================================================== */

#ifndef PCHEM_H
#define PCHEM_H

#include <gint/display.h>
#include <gint/keyboard.h>

/* -----------------------------------------------------------------------
 * fx-CG50 Screen Geometry
 * The CG50 has a 396×224 pixel color LCD.  gint addresses it as a
 * 16-bit (RGB565) VRAM framebuffer committed via dupdate().
 * ----------------------------------------------------------------------- */
#define SCREEN_W  396
#define SCREEN_H  224

/* -----------------------------------------------------------------------
 * UI Layout Constants
 * These define the vertical rhythm of our menu screens. All values in px.
 * ----------------------------------------------------------------------- */
#define HEADER_H        28      /* Height of the blue title bar            */
#define MENU_ITEM_H     30      /* Height of each menu row                 */
#define MENU_PAD_LEFT   12      /* Left padding for menu item text         */
#define MENU_PAD_TOP     4      /* Top padding within a menu item row      */
#define SCROLL_BAR_W     4      /* Width of the scroll indicator rail      */
#define CONTENT_PAD     10      /* Padding inside content/topic screens    */

/* Maximum items visible on screen (below header) */
#define MENU_VISIBLE  ((SCREEN_H - HEADER_H) / MENU_ITEM_H)

/* -----------------------------------------------------------------------
 * Color Palette (RGB565 format)
 * We define a small, consistent palette so the UI feels cohesive.
 * Colors chosen to be readable on the CG50's slightly washed-out LCD.
 * ----------------------------------------------------------------------- */
#define COL_BG          C_WHITE
#define COL_HEADER_BG   C_RGB(3, 5, 28)     /* Dark navy header bar       */
#define COL_HEADER_FG   C_WHITE              /* Header text                */
#define COL_ITEM_BG     C_WHITE              /* Normal menu row background */
#define COL_ITEM_FG     C_BLACK              /* Normal menu row text       */
#define COL_SEL_BG      C_RGB(6, 12, 28)     /* Selected row highlight     */
#define COL_SEL_FG      C_WHITE              /* Selected row text          */
#define COL_SCROLLBAR   C_RGB(18, 18, 22)    /* Scroll indicator color     */
#define COL_MATH_FG     C_BLACK              /* Math expression foreground */

/* -----------------------------------------------------------------------
 * Application State Machine
 * -----------------------------------------------------------------------
 * The add-in is structured as a flat state machine.  The main loop in
 * main.c dispatches to the appropriate screen handler based on this enum.
 *
 * State transitions:
 *   MAIN_MENU  ──[EXE]──►  TOPIC_VIEW  ──[EXIT]──►  MAIN_MENU
 *   MAIN_MENU  ──[MENU]──► (exit add-in entirely)
 *   TOPIC_VIEW ──[EXE]──►  SUBTOPIC_VIEW (future)
 *
 * The EXIT key ALWAYS goes back one level.
 * The MENU key ALWAYS exits the add-in from any screen.
 * ----------------------------------------------------------------------- */
typedef enum {
    STATE_MAIN_MENU,        /* Top-level topic selection menu              */
    STATE_TOPIC_VIEW,       /* Content screen for a selected topic         */
    STATE_SUBTOPIC_VIEW,    /* Drill-down within a topic (future use)      */
    STATE_EXIT              /* Sentinel: triggers clean shutdown           */
} AppState;

/* -----------------------------------------------------------------------
 * Number of top-level PChem topics (from the lecture slides)
 * ----------------------------------------------------------------------- */
#define NUM_TOPICS  6

/* -----------------------------------------------------------------------
 * Topic IDs — index into the topics array
 * These correspond 1:1 to the six lecture groupings in the PDF.
 * ----------------------------------------------------------------------- */
typedef enum {
    TOPIC_PIB = 0,          /* Particle in a box / free particle / wells   */
    TOPIC_COMMUTATORS,      /* Commutators, angular momentum, spin, HUP   */
    TOPIC_OSCILLATOR,       /* Harmonic oscillator and rigid rotor         */
    TOPIC_SPECTROSCOPY,     /* IR and rovibrational spectroscopy           */
    TOPIC_HYDROGEN,         /* Hydrogen atom                               */
    TOPIC_MULTIELECTRON     /* Many-electron atoms, HF, configurations    */
} TopicID;

#endif /* PCHEM_H */
