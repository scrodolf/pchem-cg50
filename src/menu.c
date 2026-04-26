/* ==========================================================================
 * menu.c — Scrolling menu system implementation
 * ==========================================================================
 *
 * VISUAL LAYOUT
 * =============
 *  ┌──────────────────────────────────────────────┐
 *  │  ██████ Physical Chemistry ██████████████████ │  ← Header bar (HEADER_H)
 *  ├──────────────────────────────────────────────┤
 *  │  1. Particle in a Box                      │▓│  ← Scroll indicator
 *  │──────────────────────────────────────────────│
 *  │ ▶2. Commutators & Spin◀  (highlighted)     │ │
 *  │──────────────────────────────────────────────│
 *  │  3. Harmonic Osc. & Rotor                  │ │
 *  │──────────────────────────────────────────────│
 *  │  4. Diatomic Spectroscopy                  │ │
 *  │──────────────────────────────────────────────│
 *  │  5. Hydrogen Atom                          │ │
 *  │──────────────────────────────────────────────│
 *  │  6. Many-Electron Atoms                    │ │
 *  └──────────────────────────────────────────────┘
 *
 * SCROLLING LOGIC
 * ===============
 * We maintain a "window" of MENU_VISIBLE items.  When the selection moves
 * past the window boundary, scroll_top adjusts to keep the selection visible.
 * This is the same pattern used in the Utilities add-in by gbl08ma.
 * ========================================================================== */

#include "menu.h"
#include <gint/display.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * menu_init — reset a menu to its default state
 * ----------------------------------------------------------------------- */
void menu_init(Menu *m, const char *title, const MenuItem *items, int count)
{
    m->title      = title;
    m->items      = items;
    m->num_items  = count;
    m->sel        = 0;
    m->scroll_top = 0;
    m->held_key   = 0;
    m->held_count = 0;
    m->jumped     = 0;
}

/* -----------------------------------------------------------------------
 * draw_header — render the top title bar
 *
 * Uses the 18px OS font for the title.  The header is drawn as a filled
 * rectangle with white text on a dark navy background.
 * ----------------------------------------------------------------------- */
static void draw_header(const char *title)
{
    /* Fill header background */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);

    /* Draw title text, vertically centered in the header bar.
     * gint's dtext() with DTEXT_LEFT positions from top-left of the glyph.
     * The 18px font is ~14px tall, so we offset by (28-14)/2 ≈ 7px. */
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, title);
}

/* -----------------------------------------------------------------------
 * draw_scroll_indicator — thin rail on the right showing scroll position
 *
 * The indicator is a small filled rectangle whose vertical position and
 * height are proportional to the visible window within the full list.
 * ----------------------------------------------------------------------- */
static void draw_scroll_indicator(const Menu *m)
{
    if (m->num_items <= MENU_VISIBLE) {
        /* Everything fits on screen — no scroll indicator needed */
        return;
    }

    int rail_x = SCREEN_W - SCROLL_BAR_W - 2;
    int rail_y = HEADER_H;
    int rail_h = SCREEN_H - HEADER_H;

    /* Thumb proportional sizing */
    int thumb_h = (MENU_VISIBLE * rail_h) / m->num_items;
    if (thumb_h < 8) thumb_h = 8;   /* Minimum visible size */

    int thumb_y = rail_y +
        (m->scroll_top * (rail_h - thumb_h)) / (m->num_items - MENU_VISIBLE);

    /* Draw the rail background (subtle gray) */
    drect(rail_x, rail_y, rail_x + SCROLL_BAR_W - 1,
          rail_y + rail_h - 1, C_RGB(28, 28, 28));

    /* Draw the thumb */
    drect(rail_x, thumb_y, rail_x + SCROLL_BAR_W - 1,
          thumb_y + thumb_h - 1, COL_SCROLLBAR);
}

/* -----------------------------------------------------------------------
 * menu_draw — render the full menu screen to VRAM
 *
 * Drawing order:
 *   1. Header bar
 *   2. Visible menu items (from scroll_top to scroll_top + MENU_VISIBLE)
 *   3. Scroll indicator
 *
 * The caller must call dupdate() after this returns.
 * ----------------------------------------------------------------------- */
void menu_draw(const Menu *m)
{
    /* 1. Header */
    draw_header(m->title);

    /* 2. Menu items */
    int visible_count = m->num_items - m->scroll_top;
    if (visible_count > MENU_VISIBLE) visible_count = MENU_VISIBLE;

    for (int i = 0; i < visible_count; i++) {
        int idx = m->scroll_top + i;         /* Absolute item index */
        int y   = HEADER_H + i * MENU_ITEM_H; /* Top of this row   */

        int is_selected = (idx == m->sel);

        /* Row background */
        int bg = is_selected ? COL_SEL_BG : COL_ITEM_BG;
        int fg = is_selected ? COL_SEL_FG : COL_ITEM_FG;
        drect(0, y, SCREEN_W - SCROLL_BAR_W - 4, y + MENU_ITEM_H - 1, bg);

        /* Separator line between items (thin gray line at bottom of row) */
        if (!is_selected) {
            drect(MENU_PAD_LEFT, y + MENU_ITEM_H - 1,
                  SCREEN_W - SCROLL_BAR_W - 6, y + MENU_ITEM_H - 1,
                  C_RGB(26, 26, 26));
        }

        /* Item text — vertically centered in the row.
         * We use the default gint font which maps to the 18px OS font. */
        int text_y = y + (MENU_ITEM_H - 14) / 2;  /* 14 ≈ glyph height */
        dtext(MENU_PAD_LEFT, text_y, fg, m->items[idx].label);

        /* Selection arrow indicator */
        if (is_selected) {
            dtext(SCREEN_W - SCROLL_BAR_W - 20, text_y, fg, "\xE6\x91");
            /* \xE6\x91 is a right-pointing triangle in the OS font.
             * If this doesn't render, we fall back to a simple ">" below. */
        }
    }

    /* 3. Scroll indicator */
    draw_scroll_indicator(m);
}

/* -----------------------------------------------------------------------
 * menu_handle_key — process a keyboard event for the menu
 *
 * Scroll logic:
 *   - UP/DOWN move the selection by 1
 *   - If the selection moves out of the visible window, scroll_top adjusts
 *   - scroll_top is clamped to [0, num_items - MENU_VISIBLE]
 *
 * Returns a MenuAction telling the caller what high-level thing happened.
 * ----------------------------------------------------------------------- */
/* -----------------------------------------------------------------------
 * v5 navigation rules:
 *   UP / DOWN     : single-step on KEYEV_DOWN, single-step on each
 *                   KEYEV_HOLD (auto-repeat) -- familiar behaviour.
 *                   IF the user keeps holding the same key for >0.5 s
 *                   (detected by counting >= 2 KEYEV_HOLD events post
 *                   DOWN, i.e. ~480 ms+), JUMP to the top (UP) or the
 *                   bottom (DOWN) and freeze further repeats until the
 *                   key is released and pressed again.
 *
 *   LEFT  / RIGHT : single-step BUT only on KEYEV_DOWN.  Hold / repeat
 *                   events are ignored, so even a continuous hold moves
 *                   the selection by exactly one item.  At the bottom
 *                   of the menu RIGHT becomes a no-op.
 *
 *   EXE / EXIT / MENU : unchanged.
 *
 * The Menu struct tracks (held_key, held_count, jumped) to drive the
 * threshold detection.  Any DOWN-edge resets these fields so a fresh
 * press always starts the timer cleanly.
 * ----------------------------------------------------------------------- */

#define LONG_PRESS_HOLD_THRESHOLD  2   /* >= this many HOLDs -> long press */

static void menu_step_up(Menu *m)
{
    if (m->sel > 0) {
        m->sel--;
        if (m->sel < m->scroll_top) m->scroll_top = m->sel;
    }
}

static void menu_step_down(Menu *m)
{
    if (m->sel < m->num_items - 1) {
        m->sel++;
        if (m->sel >= m->scroll_top + MENU_VISIBLE)
            m->scroll_top = m->sel - MENU_VISIBLE + 1;
    }
}

static void menu_jump_top(Menu *m)
{
    m->sel = 0;
    m->scroll_top = 0;
}

static void menu_jump_bottom(Menu *m)
{
    m->sel = m->num_items - 1;
    m->scroll_top = m->num_items - MENU_VISIBLE;
    if (m->scroll_top < 0) m->scroll_top = 0;
}

MenuAction menu_handle_key(Menu *m, key_event_t ev)
{
    /* Only act on press / hold events */
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return MENU_ACTION_NONE;

    /* --- Long-press tracking --- */
    if (ev.type == KEYEV_DOWN) {
        m->held_key   = ev.key;
        m->held_count = 0;
        m->jumped     = 0;
    } else if (ev.type == KEYEV_HOLD) {
        if (ev.key == m->held_key) {
            m->held_count++;
        } else {
            /* Different key without seeing DOWN first -> treat as fresh */
            m->held_key   = ev.key;
            m->held_count = 0;
            m->jumped     = 0;
        }
    }

    int is_long_press = (ev.type == KEYEV_HOLD &&
                         m->held_count >= LONG_PRESS_HOLD_THRESHOLD &&
                         !m->jumped);

    switch (ev.key) {

    /* ---- UP: single-step + long-press jump-to-top ---- */
    case KEY_UP:
        if (is_long_press) {
            menu_jump_top(m);
            m->jumped = 1;        /* one-shot per hold */
        } else if (!m->jumped) {
            menu_step_up(m);
        }
        return MENU_ACTION_NONE;

    /* ---- DOWN: single-step + long-press jump-to-bottom ---- */
    case KEY_DOWN:
        if (is_long_press) {
            menu_jump_bottom(m);
            m->jumped = 1;
        } else if (!m->jumped) {
            menu_step_down(m);
        }
        return MENU_ACTION_NONE;

    /* ---- LEFT: strict single-step UP, ignores HOLD events ---- */
    case KEY_LEFT:
        if (ev.type == KEYEV_DOWN) menu_step_up(m);
        return MENU_ACTION_NONE;

    /* ---- RIGHT: strict single-step DOWN, ignores HOLD; bottom = no-op ---- */
    case KEY_RIGHT:
        if (ev.type == KEYEV_DOWN) {
            /* No-op when already at the last item (per spec). */
            if (m->sel < m->num_items - 1) menu_step_down(m);
        }
        return MENU_ACTION_NONE;

    /* ---- Select: EXE ---- */
    case KEY_EXE:
        return MENU_ACTION_SELECT;

    /* ---- Back one level: EXIT ---- */
    case KEY_EXIT:
        return MENU_ACTION_BACK;

    /* ---- Quit add-in: MENU ---- */
    case KEY_MENU:
        return MENU_ACTION_EXIT_APP;

    default:
        return MENU_ACTION_NONE;
    }
}
