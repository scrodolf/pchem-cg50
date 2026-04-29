/* ==========================================================================
 * formula.c — Formula Guide tab
 * ==========================================================================
 *
 * Renders ALL equations from ALL topics in a single scrollable screen,
 * organised by topic section.  Each equation entry shows:
 *
 *   [Topic Title bar]
 *   Equation label
 *     [rendered math expression, with dynamic line-wrapping]
 *   ──────── (thin divider) ────────────────────────────────
 *     var1 : description …
 *     var2 : description …
 *   ════════ (entry separator) ═════════════════════════════
 *   (next equation label)
 *   …
 *
 * KEY DESIGN DECISIONS
 * --------------------
 * 1. Pool-per-equation:  render_pool_reset() is called before every
 *    equation builder so the 512-node pool never overflows across the
 *    ~40+ equations rendered in one pass.  render_equation_wrapped()
 *    (made non-static in topics.c, declared in topics.h) handles the
 *    remaining pool lifetime within that single equation.
 *
 * 2. Dual-pass rendering:  formula_draw() runs a DRAW pass (writing pixels
 *    to VRAM) followed by a MEASURE pass (y=0 baseline, no pixel writes) to
 *    compute the total content height for accurate scroll clamping.  This
 *    mirrors the pattern in topic_draw() and navigation_draw().
 *
 * 3. Word-wrap:  variable description strings use fml_draw_wrapped(), a
 *    self-contained copy of the word-wrap routine, so formula.c does not
 *    depend on the static helpers inside topics.c or navigation.c.
 * ========================================================================== */

#include "formula.h"
#include "topics.h"
#include "render.h"
#include <gint/display.h>
#include <string.h>

/* =========================================================================
 * §1  Layout constants
 * ========================================================================= */

#define FML_LINE_H       14   /* body text / var-desc line height (px)      */
#define FML_HEADING_H    20   /* topic-section heading bar height (px)      */
#define FML_CHAR_W        9   /* estimated pixel width per ASCII character   */
#define FML_MULTIBYTE_W   9   /* v6: ASCII surrogate == same width           */
#define FML_LABEL_H      16   /* equation label row height (px)             */
#define FML_EQ_INDENT     8   /* left indent for rendered math expression    */
#define FML_VARS_INDENT  12   /* left indent for variable description text   */
#define FML_DIV_GAP       4   /* vertical gap around each thin divider line  */
#define FML_ENTRY_GAP     9   /* gap between bottom of one entry and next    */
#define FML_SECTION_GAP   6   /* extra gap after last equation in a section  */

/* =========================================================================
 * §2  Private word-wrap helper
 * =========================================================================
 * Self-contained copy of the word-wrap routine (mirrors nav_draw_wrapped /
 * draw_wrapped).  Handles '\n' hard breaks, multi-byte glyph advances, and
 * the v6 greek_rewrite_word() substitution.
 * ========================================================================= */

/* Advance pointer by one glyph, respecting OS multi-byte sequences. */
static const char *fml_adv(const char *p)
{
    const unsigned char *u = (const unsigned char *)p;
    if (*u >= 0xE5 && *u <= 0xE7 && *(u + 1)) return (const char *)(u + 2);
    return (const char *)(u + 1);
}

/* Draw (draw=1) or measure (draw=0) word-wrapped text.
 * Returns the y coordinate immediately AFTER the last rendered line. */
static int fml_draw_wrapped(const char *text, int x, int start_y,
                             int max_w, int color, int draw)
{
    if (!text || !*text) return start_y;

    char  lbuf[160];
    int   llen = 0, lw = 0;
    int   cy   = start_y;

    const char *p          = text;
    const char *word_start = p;
    int         word_w     = 0;
    int         word_len   = 0;

    while (1) {
        int is_end     = (*p == '\0');
        int is_space   = (*p == ' ');
        int is_newline = (*p == '\n');

        if (is_space || is_newline || is_end) {
            /* Rewrite Greek transliterations with ASCII surrogates */
            char rbuf[64];
            int  rlen = greek_rewrite_word(word_start, word_len,
                                           rbuf, (int)sizeof(rbuf));
            const char *rb = word_start;
            int         rw = word_w;
            int         rl = word_len;
            if (rlen > 0) { rb = rbuf; rl = rlen; rw = rlen * FML_CHAR_W; }

            int needed = lw + (llen > 0 ? FML_CHAR_W : 0) + rw;
            if (needed > max_w && llen > 0) {
                lbuf[llen] = '\0';
                if (draw) dtext(x, cy, color, lbuf);
                cy   += FML_LINE_H;
                llen  = 0;
                lw    = 0;
            }
            if (llen > 0 && llen + 1 < (int)sizeof(lbuf)) {
                lbuf[llen++] = ' '; lw += FML_CHAR_W;
            }
            if (rl > 0 && llen + rl < (int)sizeof(lbuf)) {
                memcpy(lbuf + llen, rb, rl);
                llen += rl; lw += rw;
            }
            if (is_newline) {
                lbuf[llen] = '\0';
                if (draw) dtext(x, cy, color, lbuf);
                cy   += FML_LINE_H;
                llen  = 0;
                lw    = 0;
            }
            if (is_end) break;

            p++;
            word_start = p;
            word_w     = 0;
            word_len   = 0;
        } else {
            const char *nx   = fml_adv(p);
            int         step = (int)(nx - p);
            word_w   += (step == 2) ? FML_MULTIBYTE_W : FML_CHAR_W;
            word_len += step;
            p         = nx;
        }
    }

    if (llen > 0) {
        lbuf[llen] = '\0';
        if (draw) dtext(x, cy, color, lbuf);
        cy += FML_LINE_H;
    }
    return cy;
}

/* =========================================================================
 * §3  Per-topic section renderer (draw-or-measure)
 * =========================================================================
 * Returns the total pixel height of the section.
 * When draw=0, x and y are ignored (only height is computed).
 * ========================================================================= */

static int formula_section(TopicID id, int x, int y, int max_w, int draw)
{
    const TopicContent *tc = topic_content(id);
    if (!tc) return 0;

    int cy    = y;
    int avail = max_w - FML_EQ_INDENT;   /* width available for the math AST */

    /* ── Topic heading bar ─────────────────────────────────────────────── */
    if (draw) {
        drect(x, cy, x + max_w - 1, cy + FML_HEADING_H - 1, COL_ACCENT);
        dtext(x + 6, cy + 3, COL_HEADER_FG, tc->title);
    }
    cy += FML_HEADING_H + 4;

    /* ── Equations ─────────────────────────────────────────────────────── */
    for (int i = 0; i < tc->num_equations; i++) {
        const EquationEntry *eq = &tc->equations[i];

        /* Equation label (e.g. "Reduced Mass") */
        if (draw) dtext(x + 4, cy, COL_ACCENT, eq->label);
        cy += FML_LABEL_H;

        /* Rendered math expression.
         * Pool is reset per equation to avoid the 512-node limit. */
        if (eq->builder) {
            render_pool_reset();
            MathNode *tree = eq->builder();
            if (tree) {
                int h = render_equation_wrapped(
                    tree, x + FML_EQ_INDENT, cy, avail, draw);
                cy += h + 3;
            }
        }

        /* Thin divider between equation and variable descriptions */
        if (draw) {
            drect(x + 4, cy + 1,
                  x + max_w - 5, cy + 1,
                  COL_DIVIDER);
        }
        cy += FML_DIV_GAP + 2;

        /* Variable / symbol descriptions (word-wrapped, muted colour) */
        if (eq->vars && *eq->vars) {
            int vw = max_w - FML_VARS_INDENT - 4;
            cy = fml_draw_wrapped(eq->vars,
                                  x + FML_VARS_INDENT, cy,
                                  vw, COL_MUTED, draw);
            cy += 2;
        }

        /* Bold separator line between entries */
        if (draw) {
            drect(x, cy + 1, x + max_w - 1, cy + 1, COL_ITEM_FG);
        }
        cy += FML_ENTRY_GAP;
    }

    /* Extra gap before the next topic section */
    cy += FML_SECTION_GAP;
    return cy - y;
}

/* =========================================================================
 * §4  Public API
 * ========================================================================= */

void formula_init(FormulaScreen *fs)
{
    fs->scroll_y   = 0;
    fs->content_h  = 0;
    fs->held_key   = 0;
    fs->held_count = 0;
    fs->jumped     = 0;
}

void formula_draw(FormulaScreen *fs)
{
    /* ── Header bar ── */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG,
          "Formula Guide -- All Equations");
    dtext(SCREEN_W - 100, 7, COL_HEADER_FG, "[EXIT] Back");

    int x     = CONTENT_PAD;
    int max_w = SCREEN_W - CONTENT_PAD * 2;

    /* ── Draw pass (scroll-offset y baseline) ── */
    int cy = HEADER_H + CONTENT_PAD - fs->scroll_y;
    for (int t = 0; t < NUM_TOPICS; t++) {
        cy += formula_section((TopicID)t, x, cy, max_w, 1);
    }

    /* ── Measure pass (y = 0 baseline, no pixel output) ── */
    int total = 0;
    for (int t = 0; t < NUM_TOPICS; t++) {
        total += formula_section((TopicID)t, 0, 0, max_w, 0);
    }
    fs->content_h = total;

    /* ── Footer bar ── */
    int fy = SCREEN_H - FOOTER_H;
    drect(0, fy, SCREEN_W - 1, SCREEN_H - 1, COL_HEADER_BG);
    if (fs->content_h > SCREEN_H - HEADER_H - FOOTER_H) {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "UP/DOWN: scroll   F1/F2: page   EXIT: back");
    } else {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "EXIT: back   MENU: quit");
    }
}

int formula_handle_key(FormulaScreen *fs, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    /* Long-press tracking (same v5 rules as all scrollable screens) */
    if (ev.type == KEYEV_DOWN) {
        fs->held_key   = ev.key;
        fs->held_count = 0;
        fs->jumped     = 0;
    } else {  /* KEYEV_HOLD */
        if (ev.key == fs->held_key) {
            fs->held_count++;
        } else {
            fs->held_key   = ev.key;
            fs->held_count = 0;
            fs->jumped     = 0;
        }
    }

    int is_long = (ev.type == KEYEV_HOLD &&
                   fs->held_count >= 2 && !fs->jumped);

    int page  = SCREEN_H - HEADER_H - FOOTER_H;
    int limit = fs->content_h - page;
    if (limit < 0) limit = 0;

    switch (ev.key) {

    case KEY_UP:
        if (is_long) {
            fs->scroll_y = 0;
            fs->jumped   = 1;
        } else if (!fs->jumped) {
            fs->scroll_y -= 18;
            if (fs->scroll_y < 0) fs->scroll_y = 0;
        }
        return 0;

    case KEY_DOWN:
        if (is_long) {
            fs->scroll_y = limit;
            fs->jumped   = 1;
        } else if (!fs->jumped) {
            fs->scroll_y += 18;
            if (fs->scroll_y > limit) fs->scroll_y = limit;
        }
        return 0;

    case KEY_LEFT:
        if (ev.type == KEYEV_DOWN) {
            fs->scroll_y -= 18;
            if (fs->scroll_y < 0) fs->scroll_y = 0;
        }
        return 0;

    case KEY_RIGHT:
        if (ev.type == KEYEV_DOWN && fs->scroll_y < limit) {
            fs->scroll_y += 18;
            if (fs->scroll_y > limit) fs->scroll_y = limit;
        }
        return 0;

    case KEY_F1:
        fs->scroll_y -= page;
        if (fs->scroll_y < 0) fs->scroll_y = 0;
        return 0;

    case KEY_F2:
        fs->scroll_y += page;
        if (fs->scroll_y > limit) fs->scroll_y = limit;
        return 0;

    case KEY_EXIT:  return 1;
    case KEY_MENU:  return 2;
    default:        return 0;
    }
}
