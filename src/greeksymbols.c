/* ==========================================================================
 * greeksymbols.c — Greek Symbols Dictionary tab
 * ==========================================================================
 *
 * Scrollable dictionary that programmatically and visually links every
 * Greek glyph used in the add-in to:
 *   • The glyph itself (ASCII surrogate rendered via sym())
 *   • Its spelled-out name
 *   • Its substitute character (identical to the surrogate in v6)
 *   • Its meaning(s) with subject-area context
 *
 * VISUAL LAYOUT PER ENTRY (396 × 224 px screen)
 * -----------------------------------------------
 *  x+4   [glyph]   x+20 [name]   x+(20+name_w+4)  ->  sub: '[x]'
 *        x+26 [Meaning 1 — word-wrapped to available width]
 *             [Context 1 — muted, indented 4 px further]
 *        x+26 [Meaning 2 — only when a 2nd meaning exists]
 *             [Context 2 — muted]
 *  ─────────────────────────────────────────────────────────────────────
 *
 * Two section headings divide the entries:
 *   "Lowercase Greek Symbols"
 *   "Uppercase Greek / Operators"
 *
 * ASCII SURROGATE NOTE
 * --------------------
 * In the v6 rendering system every Greek letter is replaced by an ASCII
 * surrogate at the symbol-table level (e.g. psi->'y', phi->'f', mu->'u').
 * sym(name) is called at draw time to retrieve the surrogate, keeping the
 * dictionary automatically consistent with the sym table in render.c.
 * ========================================================================== */

#include "greeksymbols.h"
#include "render.h"
#include <gint/display.h>
#include <string.h>

/* =========================================================================
 * §1  Layout constants
 * ========================================================================= */

#define GK_LINE_H       14   /* line height for all body text (px)          */
#define GK_HEADING_H    20   /* section heading bar height (px)             */
#define GK_CHAR_W        9   /* estimated pixel width per ASCII character    */
#define GK_GLYPH_COL     4   /* x offset for the surrogate glyph column     */
#define GK_NAME_COL     20   /* x offset for the spelled-out name           */
#define GK_MEAN_COL     26   /* x offset for meaning / context lines        */
#define GK_CTX_INDENT    4   /* extra indent for context vs meaning text     */
#define GK_DIV_GAP       4   /* vertical gap after entry divider            */
#define GK_ENTRY_GAP     5   /* gap between consecutive entries             */
#define GK_SECTION_GAP   6   /* extra gap after the last entry in a section */

/* =========================================================================
 * §2  Data model
 * ========================================================================= */

typedef struct {
    const char *name;       /* sym-table key AND display name  e.g. "psi"  */
    const char *meaning1;   /* primary meaning (required)                   */
    const char *context1;   /* subject-area context, or NULL                */
    const char *meaning2;   /* second meaning, or NULL if only one          */
    const char *context2;   /* context for meaning2, or NULL                */
} GreekEntry;

/* ──────────────────────────────────────────────────────────────────────────
 * Lowercase Greek
 * ────────────────────────────────────────────────────────────────────────── */
static const GreekEntry lower_greek[] = {

    { "alpha",
      "Spin-up eigenstate |alpha>",
      "(Quantum Mechanics / Spin-1/2 particles)",
      "Lagrange multiplier for fixed-N constraint",
      "(Statistical Mechanics, maximising ln W)" },

    { "beta",
      "Spin-down eigenstate |beta>",
      "(Quantum Mechanics / Spin-1/2 particles)",
      "beta = 1/(kT): Lagrange multiplier (energy)",
      "(Statistical Mechanics, Boltzmann distribution)" },

    { "epsilon",
      "Vacuum permittivity epsilon_0",
      "(Electrostatics: Coulomb potential in H-atom)",
      "Energy of level i: epsilon_i",
      "(Statistical Mechanics: Boltzmann factor)" },

    { "theta",
      "Polar angle theta in spherical polar coords",
      "(Hydrogen Atom / 3D Quantum Mechanics)",
      NULL, NULL },

    { "lambda",
      "Wavelength lambda",
      "(Spectroscopy / de Broglie relation E=h^2/2mL^2)",
      NULL, NULL },

    { "mu",
      "Reduced mass mu = m1*m2 / (m1 + m2)",
      "(Classical Mechanics, Diatomic Spectroscopy)",
      "Electric dipole moment mu",
      "(IR Selection Rule: d(mu)/dt != 0)" },

    { "nu",
      "Frequency nu (Hz)",
      "(Spectroscopy: IR, rotational, vibrational)",
      NULL, NULL },

    { "pi",
      "Mathematical constant pi ~ 3.14159",
      "(Appears in Bohr radius, rotational const., etc.)",
      NULL, NULL },

    { "rho",
      "Electron-nucleus distance rho",
      "(Hydrogen Atom Hamiltonian, Coulomb potential)",
      NULL, NULL },

    { "phi",
      "Azimuthal angle phi in spherical polar coords",
      "(Hydrogen Atom / 3D Quantum Mechanics)",
      "Atomic orbital phi_1s used in LCAO-MO",
      "(Many-Electron Atoms / Molecular Orbital Theory)" },

    { "chi",
      "Spin-orbital chi_i = spatial orb. x spin func.",
      "(Slater Determinant, Many-Electron Atoms)",
      NULL, NULL },

    { "psi",
      "Wavefunction psi_n (1-electron)",
      "(Quantum Mechanics: Schrodinger equation)",
      "Hybrid orbital e.g. psi_sp,1",
      "(sp / sp2 / sp3 Hybridization, Many-Electron)" },

    { "omega",
      "Angular frequency omega = 2*pi*nu (rad/s)",
      "(Harmonic Oscillator / Diatomic Spectroscopy)",
      NULL, NULL },
};

/* ──────────────────────────────────────────────────────────────────────────
 * Uppercase Greek and Operator Symbols
 * ────────────────────────────────────────────────────────────────────────── */
static const GreekEntry upper_greek[] = {

    { "Delta",
      "Energy gap Delta E (e.g. E_LUMO - E_HOMO)",
      "(Particle in a Box / Spectroscopy / Boltzmann)",
      "Change in quantum number: Delta n = +/- 1",
      "(Selection Rules for IR absorption/emission)" },

    { "Pi",
      "Product operator Pi_j over all levels j",
      "(Stat. Mech.: microstate weight W = N!/Pi_j a_j!)",
      NULL, NULL },

    { "Sigma",
      "Summation operator Sigma_i over levels / particles",
      "(Hamiltonians / Statistical Mechanics)",
      NULL, NULL },

    { "Phi",
      "Trial wavefunction Phi for Variation Theorem",
      "(Many-Electron Atoms / Hartree-Fock / SCF)",
      NULL, NULL },

    { "Psi",
      "Total N-electron wavefunction Psi(1, 2, ..., N)",
      "(Many-Electron Atoms / Slater Determinant)",
      NULL, NULL },

    { "nabla",
      "Laplacian operator nabla^2 (kinetic energy)",
      "(All Hamiltonians: T = -(hbar^2/2m) nabla^2)",
      NULL, NULL },
};

#define NUM_LOWER  ((int)(sizeof(lower_greek)  / sizeof(lower_greek[0])))
#define NUM_UPPER  ((int)(sizeof(upper_greek)  / sizeof(upper_greek[0])))

/* =========================================================================
 * §3  Private render helpers
 * ========================================================================= */

/* Word-wrap a text string inside [x, x+max_w].
 * draw=1 writes pixels; draw=0 measures only.
 * Returns y coordinate after the last line. */
static int gk_text_wrap(const char *text, int x, int y,
                        int max_w, int color, int draw)
{
    if (!text || !*text) return y;

    char  buf[120];
    int   blen = 0, bw = 0;
    int   cy   = y;
    const char *p = text;

    while (*p) {
        /* Scan one word */
        const char *ws = p;
        int ww = 0, wl = 0;
        while (*p && *p != ' ' && *p != '\n') { ww += GK_CHAR_W; wl++; p++; }

        int sp_w = (blen > 0) ? GK_CHAR_W : 0;
        if (blen > 0 && bw + sp_w + ww > max_w) {
            buf[blen] = '\0';
            if (draw) dtext(x, cy, color, buf);
            cy  += GK_LINE_H;
            blen = 0; bw = 0;
        }
        if (blen > 0 && blen + 1 < (int)sizeof(buf)) {
            buf[blen++] = ' '; bw += GK_CHAR_W;
        }
        if (wl > 0 && blen + wl < (int)sizeof(buf)) {
            memcpy(buf + blen, ws, wl);
            blen += wl; bw += ww;
        }
        if (*p == '\n') {
            buf[blen] = '\0';
            if (draw) dtext(x, cy, color, buf);
            cy  += GK_LINE_H;
            blen = 0; bw = 0; p++;
        } else if (*p == ' ') {
            p++;
        }
    }
    if (blen > 0) {
        buf[blen] = '\0';
        if (draw) dtext(x, cy, color, buf);
        cy += GK_LINE_H;
    }
    return cy;
}

/* Render a section heading bar ("Lowercase Greek Symbols", etc.).
 * Returns height consumed. */
static int gk_heading(const char *title, int x, int y,
                      int max_w, int draw)
{
    if (draw) {
        drect(x, y, x + max_w - 1, y + GK_HEADING_H - 1, COL_ACCENT);
        dtext(x + 6, y + 3, COL_HEADER_FG, title);
    }
    return GK_HEADING_H + 4;
}

/* Render (or measure) one dictionary entry.  Returns height consumed. */
static int gk_entry(const GreekEntry *e, int x, int y, int max_w, int draw)
{
    int cy = y;

    /* ── Line 1: [glyph]  [name]  ->  sub: '[x]' ──────────────────────── */
    const char *glyph = sym(e->name);

    if (draw) {
        /* Glyph in accent colour — this IS the character rendered in eqs. */
        dtext(x + GK_GLYPH_COL, cy, COL_ACCENT, glyph);

        /* Spelled-out name */
        dtext(x + GK_NAME_COL, cy, COL_ITEM_FG, e->name);

        /* "  ->  sub: 'x'" annotation (muted, right of name) */
        int name_w = (int)strlen(e->name) * GK_CHAR_W;
        char sub_buf[32];
        int  nb = 0;
        /* Build annotation string manually (no snprintf on sh-elf) */
        sub_buf[nb++] = ' '; sub_buf[nb++] = ' ';
        sub_buf[nb++] = '-'; sub_buf[nb++] = '>';
        sub_buf[nb++] = ' '; sub_buf[nb++] = 's'; sub_buf[nb++] = 'u';
        sub_buf[nb++] = 'b'; sub_buf[nb++] = ':'; sub_buf[nb++] = ' ';
        sub_buf[nb++] = '\'';
        for (const char *g = glyph; *g && nb < 28; g++) sub_buf[nb++] = *g;
        sub_buf[nb++] = '\'';
        sub_buf[nb]   = '\0';
        dtext(x + GK_NAME_COL + name_w, cy, COL_MUTED, sub_buf);
    }
    cy += GK_LINE_H + 2;

    /* ── Meaning 1 (word-wrapped) ──────────────────────────────────────── */
    int mean_w = max_w - GK_MEAN_COL - 4;
    if (e->meaning1) {
        cy = gk_text_wrap(e->meaning1, x + GK_MEAN_COL, cy,
                          mean_w, COL_ITEM_FG, draw);
    }
    /* Context 1 (muted, slightly more indented) */
    if (e->context1) {
        cy = gk_text_wrap(e->context1, x + GK_MEAN_COL + GK_CTX_INDENT, cy,
                          mean_w - GK_CTX_INDENT, COL_MUTED, draw);
    }

    /* ── Meaning 2 (optional) ──────────────────────────────────────────── */
    if (e->meaning2) {
        cy += 2;  /* extra visual separation before 2nd meaning */
        cy = gk_text_wrap(e->meaning2, x + GK_MEAN_COL, cy,
                          mean_w, COL_ITEM_FG, draw);
        if (e->context2) {
            cy = gk_text_wrap(e->context2,
                              x + GK_MEAN_COL + GK_CTX_INDENT, cy,
                              mean_w - GK_CTX_INDENT, COL_MUTED, draw);
        }
    }

    /* ── Entry divider ─────────────────────────────────────────────────── */
    if (draw) drect(x, cy + 2, x + max_w - 1, cy + 2, COL_DIVIDER);
    cy += GK_DIV_GAP + GK_ENTRY_GAP;

    return cy - y;
}

/* Draw-or-measure one entire section (heading + all entries). */
static int gk_section(const char *title,
                       const GreekEntry *entries, int count,
                       int x, int y, int max_w, int draw)
{
    int cy = y;
    cy += gk_heading(title, x, cy, max_w, draw);
    for (int i = 0; i < count; i++) {
        cy += gk_entry(&entries[i], x, cy, max_w, draw);
    }
    cy += GK_SECTION_GAP;
    return cy - y;
}

/* =========================================================================
 * §4  Public API
 * ========================================================================= */

void greek_init(GreekScreen *gs)
{
    gs->scroll_y   = 0;
    gs->content_h  = 0;
    gs->held_key   = 0;
    gs->held_count = 0;
    gs->jumped     = 0;
}

void greek_draw(GreekScreen *gs)
{
    /* ── Header bar ── */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, "Greek Symbols Dictionary");
    dtext(SCREEN_W - 100, 7, COL_HEADER_FG, "[EXIT] Back");

    int x     = CONTENT_PAD;
    int max_w = SCREEN_W - CONTENT_PAD * 2;

    /* ── Draw pass ── */
    int cy = HEADER_H + CONTENT_PAD - gs->scroll_y;
    cy += gk_section("Lowercase Greek Symbols",
                     lower_greek, NUM_LOWER, x, cy, max_w, 1);
    cy += gk_section("Uppercase Greek / Operators",
                     upper_greek, NUM_UPPER, x, cy, max_w, 1);

    /* ── Measure pass (y = 0 baseline, no pixel output) ── */
    int total = 0;
    total += gk_section("Lowercase Greek Symbols",
                        lower_greek, NUM_LOWER, 0, 0, max_w, 0);
    total += gk_section("Uppercase Greek / Operators",
                        upper_greek, NUM_UPPER, 0, 0, max_w, 0);
    gs->content_h = total;

    /* ── Footer bar ── */
    int fy = SCREEN_H - FOOTER_H;
    drect(0, fy, SCREEN_W - 1, SCREEN_H - 1, COL_HEADER_BG);
    if (gs->content_h > SCREEN_H - HEADER_H - FOOTER_H) {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "UP/DOWN: scroll   F1/F2: page   EXIT: back");
    } else {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "EXIT: back   MENU: quit");
    }
}

int greek_handle_key(GreekScreen *gs, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    if (ev.type == KEYEV_DOWN) {
        gs->held_key   = ev.key;
        gs->held_count = 0;
        gs->jumped     = 0;
    } else {  /* KEYEV_HOLD */
        if (ev.key == gs->held_key) {
            gs->held_count++;
        } else {
            gs->held_key   = ev.key;
            gs->held_count = 0;
            gs->jumped     = 0;
        }
    }

    int is_long = (ev.type == KEYEV_HOLD &&
                   gs->held_count >= 2 && !gs->jumped);

    int page  = SCREEN_H - HEADER_H - FOOTER_H;
    int limit = gs->content_h - page;
    if (limit < 0) limit = 0;

    switch (ev.key) {

    case KEY_UP:
        if (is_long) {
            gs->scroll_y = 0;
            gs->jumped   = 1;
        } else if (!gs->jumped) {
            gs->scroll_y -= 18;
            if (gs->scroll_y < 0) gs->scroll_y = 0;
        }
        return 0;

    case KEY_DOWN:
        if (is_long) {
            gs->scroll_y = limit;
            gs->jumped   = 1;
        } else if (!gs->jumped) {
            gs->scroll_y += 18;
            if (gs->scroll_y > limit) gs->scroll_y = limit;
        }
        return 0;

    case KEY_LEFT:
        if (ev.type == KEYEV_DOWN) {
            gs->scroll_y -= 18;
            if (gs->scroll_y < 0) gs->scroll_y = 0;
        }
        return 0;

    case KEY_RIGHT:
        if (ev.type == KEYEV_DOWN && gs->scroll_y < limit) {
            gs->scroll_y += 18;
            if (gs->scroll_y > limit) gs->scroll_y = limit;
        }
        return 0;

    case KEY_F1:
        gs->scroll_y -= page;
        if (gs->scroll_y < 0) gs->scroll_y = 0;
        return 0;

    case KEY_F2:
        gs->scroll_y += page;
        if (gs->scroll_y > limit) gs->scroll_y = limit;
        return 0;

    case KEY_EXIT:  return 1;
    case KEY_MENU:  return 2;
    default:        return 0;
    }
}
