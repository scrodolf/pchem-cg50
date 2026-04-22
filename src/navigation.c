/* ==========================================================================
 * navigation.c - Global keyword/symbol navigator
 * ==========================================================================
 *
 * Shows ALL keywords from every topic in one long scrollable list,
 * divided by topic.  Each section is:
 *
 *   [Topic Title heading]
 *     keyword 1   short definition ...
 *     keyword 2   short definition ...
 *     ...
 *     Symbols used: psi  phi  Delta  omega  mu  ...
 *
 * Uses the same measure-then-draw scrolling pattern as topic_draw().
 * ========================================================================== */

#include "navigation.h"
#include "topics.h"
#include "render.h"
#include <gint/display.h>
#include <string.h>

/* Layout constants (local, purposely mild to fit lots of text) */
#define NAV_LINE_H        14
#define NAV_HEADING_H     20
#define NAV_CHAR_W         9
#define NAV_MULTIBYTE_W   10
#define NAV_PARA_GAP       4

/* Advance pointer by one glyph (multi-byte aware) */
static const char *nav_advance_glyph(const char *p)
{
    const unsigned char *u = (const unsigned char *)p;
    if (*u >= 0xE5 && *u <= 0xE7 && *(u + 1)) return (const char *)(u + 2);
    return (const char *)(u + 1);
}

/* Word-wrap draw-or-measure helper -- mirrors the one in topics.c
 * but kept private so navigation.c is self-contained. */
static int nav_draw_wrapped(const char *text, int x, int start_y,
                            int max_w, int color, int draw)
{
    if (!text) return start_y;

    char line_buf[160];
    int line_len = 0, line_w = 0;
    int cur_y = start_y;

    const char *p = text;
    const char *word_start = p;
    int word_w = 0, word_len = 0;

    while (1) {
        int is_end = (*p == '\0');
        int is_space = (*p == ' ');
        int is_newline = (*p == '\n');

        if (is_space || is_newline || is_end) {
            int needed = line_w + (line_len > 0 ? NAV_CHAR_W : 0) + word_w;
            if (needed > max_w && line_len > 0) {
                line_buf[line_len] = '\0';
                if (draw) dtext(x, cur_y, color, line_buf);
                cur_y += NAV_LINE_H;
                line_len = 0;
                line_w = 0;
            }
            if (line_len > 0 && line_len + 1 < (int)sizeof(line_buf)) {
                line_buf[line_len++] = ' ';
                line_w += NAV_CHAR_W;
            }
            if (word_len > 0 &&
                line_len + word_len < (int)sizeof(line_buf)) {
                memcpy(line_buf + line_len, word_start, word_len);
                line_len += word_len;
                line_w += word_w;
            }
            if (is_newline) {
                line_buf[line_len] = '\0';
                if (draw) dtext(x, cur_y, color, line_buf);
                cur_y += NAV_LINE_H;
                line_len = 0;
                line_w = 0;
            }
            if (is_end) break;
            p++;
            word_start = p;
            word_w = 0;
            word_len = 0;
        } else {
            const char *next = nav_advance_glyph(p);
            int step = (int)(next - p);
            word_w += (step == 2) ? NAV_MULTIBYTE_W : NAV_CHAR_W;
            word_len += step;
            p = next;
        }
    }
    if (line_len > 0) {
        line_buf[line_len] = '\0';
        if (draw) dtext(x, cur_y, color, line_buf);
        cur_y += NAV_LINE_H;
    }
    return cur_y;
}

/* =========================================================================
 * Per-topic symbol lists
 * =========================================================================
 * These are the OS-font symbols actually used in each topic's equations.
 * Listed as one string because the navigator only displays them; the
 * symbol bytes are already loaded into the symbol table by sym_table_init.
 * ========================================================================= */

/* For each topic, a list of "name (symbol)" entries -- rendered by pulling
 * the name through sym() to get the OS bytes for the glyph column. */
typedef struct {
    const char *name;   /* key used in sym_table_init */
    const char *desc;   /* human-readable label */
} NavSymbol;

static const NavSymbol syms_pib[] = {
    { "psi",    "psi  wavefunction" },
    { "pi",     "pi   constant pi" },
    { "Delta",  "Delta  energy gap (HOMO-LUMO)" },
};

static const NavSymbol syms_commutators[] = {
    { "Delta",  "Delta  uncertainty / change" },
    { "alpha",  "alpha  spin-up state" },
    { "beta",   "beta   spin-down state" },
};

static const NavSymbol syms_oscillator[] = {
    { "omega",  "omega  angular frequency" },
};

static const NavSymbol syms_spectroscopy[] = {
    { "mu",     "mu     reduced mass / dipole" },
    { "nu",     "nu     frequency" },
    { "lambda", "lambda wavelength" },
    { "pi",     "pi     constant pi" },
    { "Delta",  "Delta  selection-rule change" },
    { "pm",     "pm     plus-or-minus" },
};

static const NavSymbol syms_hydrogen[] = {
    { "rho",    "rho     radial coordinate" },
    { "theta",  "theta   polar angle" },
    { "phi",    "phi     azimuthal angle" },
    { "psi",    "psi     wavefunction" },
    { "pi",     "pi      constant pi" },
    { "epsilon","epsilon vacuum permittivity" },
};

static const NavSymbol syms_multielectron[] = {
    { "Psi",    "Psi     total wavefunction" },
    { "psi",    "psi     1-electron / hybrid orbital" },
    { "Phi",    "Phi     trial wavefunction" },
    { "phi",    "phi     atomic orbital (LCAO)" },
    { "chi",    "chi     spin-orbital (Slater)" },
    { "nabla",  "nabla   gradient operator" },
    { "epsilon","epsilon vacuum permittivity" },
};

/* Topic -> symbol list dispatch */
static const NavSymbol *topic_symbols(TopicID id, int *count)
{
    switch (id) {
    case TOPIC_PIB:
        *count = (int)(sizeof(syms_pib) / sizeof(syms_pib[0]));
        return syms_pib;
    case TOPIC_COMMUTATORS:
        *count = (int)(sizeof(syms_commutators) / sizeof(syms_commutators[0]));
        return syms_commutators;
    case TOPIC_OSCILLATOR:
        *count = (int)(sizeof(syms_oscillator) / sizeof(syms_oscillator[0]));
        return syms_oscillator;
    case TOPIC_SPECTROSCOPY:
        *count = (int)(sizeof(syms_spectroscopy)
                       / sizeof(syms_spectroscopy[0]));
        return syms_spectroscopy;
    case TOPIC_HYDROGEN:
        *count = (int)(sizeof(syms_hydrogen) / sizeof(syms_hydrogen[0]));
        return syms_hydrogen;
    case TOPIC_MULTIELECTRON:
        *count = (int)(sizeof(syms_multielectron)
                       / sizeof(syms_multielectron[0]));
        return syms_multielectron;
    default:
        *count = 0;
        return NULL;
    }
}

/* =========================================================================
 * Section renderer (draw-or-measure)
 * ========================================================================= */

/* Draw the heading bar for a topic section */
static int nav_draw_heading(const char *title, int x, int y, int max_w,
                            int draw)
{
    if (draw) {
        drect(x, y, x + max_w - 1, y + NAV_HEADING_H - 1, COL_ACCENT);
        dtext(x + 4, y + 3, COL_HEADER_FG, title);
    }
    return NAV_HEADING_H + 2;
}

/* Draw-or-measure the full content of one topic section */
static int nav_section(TopicID id, int x, int y, int max_w, int draw)
{
    const TopicContent *tc = topic_content(id);
    if (!tc) return 0;

    int cy = y;

    /* Topic heading */
    cy += nav_draw_heading(tc->title, x, cy, max_w, draw);

    /* Keyword list */
    for (int i = 0; i < tc->num_keywords; i++) {
        const KeywordEntry *kw = &tc->keywords[i];
        if (draw) dtext(x + 4, cy, COL_ACCENT, kw->name);
        cy += NAV_LINE_H + 1;
        cy = nav_draw_wrapped(kw->definition, x + 16, cy,
                              max_w - 16, COL_ITEM_FG, draw);
        cy += 2;
    }

    /* Symbol list */
    int sym_count = 0;
    const NavSymbol *syms = topic_symbols(id, &sym_count);
    if (sym_count > 0) {
        cy += 3;
        if (draw) dtext(x + 4, cy, COL_MUTED, "Symbols used:");
        cy += NAV_LINE_H + 1;
        for (int i = 0; i < sym_count; i++) {
            if (draw) {
                /* Glyph column (OS multi-byte) */
                dtext(x + 16, cy, COL_ITEM_FG, sym(syms[i].name));
                /* Description */
                dtext(x + 40, cy, COL_ITEM_FG, syms[i].desc);
            }
            cy += NAV_LINE_H;
        }
    }

    cy += NAV_PARA_GAP + 4;
    return cy - y;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void navigation_init(NavigationScreen *ns)
{
    ns->scroll_y = 0;
    ns->content_h = 0;
}

void navigation_draw(NavigationScreen *ns)
{
    /* Header */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, "Navigation -- All Keywords & Symbols");
    dtext(SCREEN_W - 100, 7, COL_HEADER_FG, "[EXIT] Back");

    int x     = CONTENT_PAD;
    int max_w = SCREEN_W - CONTENT_PAD * 2;
    int y     = HEADER_H + CONTENT_PAD - ns->scroll_y;

    int cy = y;
    for (int t = 0; t < NUM_TOPICS; t++) {
        cy += nav_section((TopicID)t, x, cy, max_w, 1);
    }

    /* Measure pass (independent) for accurate content_h */
    int total = 0;
    for (int t = 0; t < NUM_TOPICS; t++) {
        total += nav_section((TopicID)t, 0, 0, max_w, 0);
    }
    ns->content_h = total;

    /* Footer hint */
    int fy = SCREEN_H - FOOTER_H;
    drect(0, fy, SCREEN_W - 1, SCREEN_H - 1, COL_HEADER_BG);
    if (ns->content_h > SCREEN_H - HEADER_H - FOOTER_H) {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "UP/DOWN: scroll   F1/F2: page   EXIT: back");
    } else {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "EXIT: back   MENU: quit");
    }
}

int navigation_handle_key(NavigationScreen *ns, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    int page  = SCREEN_H - HEADER_H - FOOTER_H;
    int limit = ns->content_h - page;
    if (limit < 0) limit = 0;

    switch (ev.key) {
    case KEY_UP:
        ns->scroll_y -= 18;
        if (ns->scroll_y < 0) ns->scroll_y = 0;
        return 0;
    case KEY_DOWN:
        ns->scroll_y += 18;
        if (ns->scroll_y > limit) ns->scroll_y = limit;
        return 0;
    case KEY_F1:
        ns->scroll_y -= page;
        if (ns->scroll_y < 0) ns->scroll_y = 0;
        return 0;
    case KEY_F2:
        ns->scroll_y += page;
        if (ns->scroll_y > limit) ns->scroll_y = limit;
        return 0;
    case KEY_EXIT:
        return 1;
    case KEY_MENU:
        return 2;
    default:
        return 0;
    }
}
