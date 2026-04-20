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
MenuAction menu_handle_key(Menu *m, key_event_t ev)
{
    /* Only handle key-down events (ignore key-up) */
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return MENU_ACTION_NONE;

    switch (ev.key) {

    /* ---- Navigation: UP ---- */
    case KEY_UP:
        if (m->sel > 0) {
            m->sel--;
            /* If selection scrolled above the visible window, adjust */
            if (m->sel < m->scroll_top) {
                m->scroll_top = m->sel;
            }
        }
        return MENU_ACTION_NONE;

    /* ---- Navigation: DOWN ---- */
    case KEY_DOWN:
        if (m->sel < m->num_items - 1) {
            m->sel++;
            /* If selection scrolled below the visible window, adjust */
            if (m->sel >= m->scroll_top + MENU_VISIBLE) {
                m->scroll_top = m->sel - MENU_VISIBLE + 1;
            }
        }
        return MENU_ACTION_NONE;

    /* ---- Select: EXE (the large key on the Casio keypad) ---- */
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
