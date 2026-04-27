/* ==========================================================================
 * pchem.h — Global definitions for the PChem add-in
 * ==========================================================================
 * This header is included by every translation unit. It defines:
 *   - Screen geometry constants (fx-CG50 = 396x224 pixels)
 *   - The top-level application state machine enum
 *   - Topic and subtopic identifiers
 *   - Forward declarations shared across modules
 *
 * UPDATED for v2:
 *   - Added STATE_SUBMENU + STATE_NAVIGATION
 *   - Added SubtopicID (DESCRIPTIONS / EQUATIONS / KEYWORDS)
 *   - Added sentinel TOPIC_NAVIGATION_MARKER so the main menu can route
 *     the "Navigation" entry to a dedicated screen without adding a
 *     parallel MenuItem type.
 * ========================================================================== */

#ifndef PCHEM_H
#define PCHEM_H

#include <gint/display.h>
#include <gint/keyboard.h>

/* -----------------------------------------------------------------------
 * fx-CG50 Screen Geometry
 * The CG50 has a 396x224 pixel color LCD.  gint addresses it as a
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
#define FOOTER_H        18      /* Height of the bottom hint bar           */

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
#define COL_ACCENT      C_RGB(6, 12, 28)     /* Section heading accent     */
#define COL_MUTED       C_RGB(16, 16, 20)    /* Secondary / hint text      */
#define COL_DIVIDER     C_RGB(24, 24, 28)    /* Thin separator line color  */

/* -----------------------------------------------------------------------
 * Application State Machine
 * -----------------------------------------------------------------------
 * The add-in is now a four-level state machine:
 *
 *   MAIN_MENU  -[EXE on topic]->  SUBMENU  -[EXE on subtopic]->  TOPIC_VIEW
 *        |                           |                                |
 *        |                           +-------[EXIT]-------------------+
 *        |                                                    <-- back to SUBMENU
 *        +-[EXE on Navigation]--> NAVIGATION -[EXIT]-> MAIN_MENU
 *
 *   MENU  ALWAYS exits the add-in from any state.
 *   EXIT  ALWAYS goes back one level.
 * ----------------------------------------------------------------------- */
typedef enum {
    STATE_MAIN_MENU,        /* Top-level topic selection + Navigation entry */
    STATE_SUBMENU,          /* Per-topic submenu: Desc/Eqns/Keywords        */
    STATE_TOPIC_VIEW,       /* Actual content screen for one subtopic       */
    STATE_NAVIGATION,       /* Global keyword/symbol navigator              */
    STATE_EXIT              /* Sentinel: triggers clean shutdown            */
} AppState;

/* -----------------------------------------------------------------------
 * Number of top-level PChem topics (from the lecture slides)
 * v4: bumped from 6 -> 7 to add Statistical Mechanics (Lecture 13)
 * ----------------------------------------------------------------------- */
#define NUM_TOPICS  7

/* Total entries on the main menu = topics + 1 (Navigation) */
#define NUM_MAIN_MENU_ENTRIES  (NUM_TOPICS + 1)

/* -----------------------------------------------------------------------
 * Topic IDs - index into the topics array
 * These correspond 1:1 to the lecture groupings in the PDF.
 *
 * TOPIC_NAVIGATION_MARKER is a SENTINEL value used only to tag the
 * "Navigation" entry in the main menu array - it is never used as an
 * index into content tables.  Chosen high so it cannot collide with
 * any real topic index.
 * ----------------------------------------------------------------------- */
typedef enum {
    TOPIC_PIB = 0,          /* Particle in a box / free particle / wells   */
    TOPIC_COMMUTATORS,      /* Commutators, angular momentum, spin, HUP    */
    TOPIC_OSCILLATOR,       /* Harmonic oscillator and rigid rotor         */
    TOPIC_SPECTROSCOPY,     /* IR and rovibrational spectroscopy           */
    TOPIC_HYDROGEN,         /* Hydrogen atom                               */
    TOPIC_MULTIELECTRON,    /* Many-electron atoms, HF, configurations     */
    TOPIC_STATMECH,         /* Statistical mechanics: Boltzmann, partition */

    TOPIC_NAVIGATION_MARKER = 100  /* sentinel for the main-menu router */
} TopicID;

/* -----------------------------------------------------------------------
 * SubtopicID - three subsections present inside every topic's submenu.
 * Stored in SubMenuScreen to drive which content view is rendered.
 * ----------------------------------------------------------------------- */
typedef enum {
    SUBTOPIC_DESCRIPTIONS = 0,  /* paragraph-style overview                 */
    SUBTOPIC_EQUATIONS,         /* equations + variable descriptions        */
    SUBTOPIC_KEYWORDS,          /* key terms (short definitions)            */
    NUM_SUBTOPICS               /* sentinel - keep last                     */
} SubtopicID;

/* -----------------------------------------------------------------------
 * GREEK & MATH GLYPH MACROS
 * -----------------------------------------------------------------------
 * v6: switched from OS multi-byte sequences (\xE5xx) to ASCII surrogates.
 * gint's default fx-CG50 font only renders ASCII reliably; the OS Greek
 * page produced blank space on real hardware (visible as gaps in
 * equations and prose).  We use the same TeX-style mnemonic surrogates
 * as the symbol table in render.c: phi=f, psi=y, mu=u, beta=b, etc.
 *
 * These macros remain useful so prose strings stay self-documenting and
 * can be migrated to true bitmap glyphs later without changing source.
 * Usage:  "energy gap " G_DELTA "E"  ->  "energy gap DE"
 * ----------------------------------------------------------------------- */
/* Lowercase Greek */
#define G_ALPHA   "a"
#define G_BETA    "b"
#define G_GAMMA   "g"
#define G_DELTA   "d"
#define G_EPSILON "e"
#define G_THETA   "O"
#define G_LAMBDA  "L"
#define G_MU      "u"
#define G_NU      "v"
#define G_PI      "p"
#define G_RHO     "r"
#define G_SIGMA   "s"
#define G_TAU     "t"
#define G_PHI     "f"
#define G_CHI     "c"
#define G_PSI     "y"
#define G_OMEGA   "w"

/* Uppercase Greek */
#define G_GAMMA_U "G"
#define G_DELTA_U "D"
#define G_THETA_U "Q"
#define G_LAMBDA_U "A"
#define G_PI_U    "P"
#define G_SIGMA_U "S"
#define G_PHI_U   "F"
#define G_PSI_U   "Y"
#define G_OMEGA_U "W"

/* Math operators */
#define G_PM      "+/-"
#define G_TIMES   "x"
#define G_LEQ     "<="
#define G_GEQ     ">="
#define G_NEQ     "!="
#define G_INF     "inf"
#define G_RARR    "->"

#endif /* PCHEM_H */
