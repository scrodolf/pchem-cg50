/* ==========================================================================
 * topics.c — Topic screen content and dispatch
 * ==========================================================================
 * Each topic gets a scrollable content screen that mixes explanatory text
 * with rendered math expressions.  For now, we implement:
 *   - The data tables (titles, descriptions)
 *   - The basic screen drawing scaffold
 *   - Stub content with a sample equation per topic (demonstrating the AST)
 *   - Vertical scrolling within content
 *
 * The full chemistry content will be added later by populating each topic's
 * build_*_content() function with MathNode trees and text blocks.
 * ========================================================================== */

#include "topics.h"
#include "render.h"
#include <gint/display.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Topic metadata tables
 * ----------------------------------------------------------------------- */

static const char *topic_titles[NUM_TOPICS] = {
    "Particle in a Box",
    "Commutators & Spin",
    "Harmonic Osc. & Rotor",
    "Diatomic Spectroscopy",
    "Hydrogen Atom",
    "Many-Electron Atoms",
};

static const char *topic_descriptions[NUM_TOPICS] = {
    "Free particle, 1D/2D/3D box, quantization,\n"
    "zero-point energy, tunneling, band theory",

    "Commutators, angular momentum operators,\n"
    "spin-1/2, Stern-Gerlach, uncertainty principle",

    "Classical & quantum harmonic oscillator,\n"
    "Hermite polynomials, rigid rotor, moment of inertia",

    "IR absorption, selection rules, dynamic dipole,\n"
    "Morse potential, P/R branches, bond lengths",

    "Radial & angular wavefunctions,\n"
    "quantum numbers, orbitals, spectral series",

    "Electron-electron repulsion, Slater determinants,\n"
    "variational theorem, Hartree-Fock, basis sets",
};

const char *topic_title(TopicID id)
{
    if (id >= 0 && id < NUM_TOPICS) return topic_titles[id];
    return "Unknown";
}

const char *topic_description(TopicID id)
{
    if (id >= 0 && id < NUM_TOPICS) return topic_descriptions[id];
    return "";
}

/* -----------------------------------------------------------------------
 * Sample equation builders (one per topic)
 * -----------------------------------------------------------------------
 * These demonstrate the AST system by constructing a representative
 * equation for each topic.  They serve as templates for the full content.
 * ----------------------------------------------------------------------- */

/* Topic 0: E_n = n²h² / 8mL²  (particle in a box energy) */
static MathNode *build_pib_sample(void)
{
    /* Numerator: n²h² */
    MathNode *n_sq = math_superscript(math_text("n"), math_number("2"));
    MathNode *h_sq = math_superscript(math_text("h"), math_number("2"));
    MathNode *num_children[] = { n_sq, h_sq };
    MathNode *numerator = math_row(num_children, 2);

    /* Denominator: 8mL² */
    MathNode *L_sq = math_superscript(math_text("L"), math_number("2"));
    MathNode *den_children[] = { math_number("8"), math_text("m"), L_sq };
    MathNode *denominator = math_row(den_children, 3);

    /* E_n = fraction */
    MathNode *E_n = math_subscript(math_text("E"), math_text("n"));
    MathNode *eq  = math_text(" = ");
    MathNode *frac = math_fraction(numerator, denominator);

    MathNode *parts[] = { E_n, eq, frac };
    return math_row(parts, 3);
}

/* Topic 1: [Â, B̂] = ÂB̂ - B̂Â  (commutator definition) */
static MathNode *build_commutator_sample(void)
{
    MathNode *A_hat = math_hat(math_text("A"));
    MathNode *B_hat = math_hat(math_text("B"));

    /* [Â, B̂] — use the B_hat node we already allocated above.
     * Note: later uses of math_hat(math_text("B")) on lines below
     * are correct — each AST node can only have one parent, so
     * separate tree positions require separate allocations. */
    MathNode *comm_inner[] = { A_hat, math_text(", "), B_hat };
    MathNode *comm_body = math_row(comm_inner, 3);

    /* We represent the square brackets as text for simplicity */
    MathNode *lbr = math_text("[");
    MathNode *rbr = math_text("]");
    MathNode *eq  = math_text(" = ");
    MathNode *AB  = math_row((MathNode*[]){
        math_hat(math_text("A")), math_hat(math_text("B"))
    }, 2);
    MathNode *minus = math_text(" - ");
    MathNode *BA = math_row((MathNode*[]){
        math_hat(math_text("B")), math_hat(math_text("A"))
    }, 2);

    MathNode *parts[] = { lbr, comm_body, rbr, eq, AB, minus, BA };
    return math_row(parts, 7);
}

/* Topic 2: E_v = ℏω(v + ½)  (harmonic oscillator energy) */
static MathNode *build_oscillator_sample(void)
{
    MathNode *E_v = math_subscript(math_text("E"), math_text("v"));
    MathNode *eq  = math_text(" = ");

    /* ℏ = h with overbar; ω = OS Greek omega via symbol table */
    MathNode *hbar  = math_bar(math_text("h"));     /* ℏ rendered as h̄  */
    MathNode *omega = math_symbol("omega");          /* ω from OS font   */

    MathNode *half = math_fraction(math_number("1"), math_number("2"));
    MathNode *v_plus_half[] = { math_text("v"), math_text(" + "), half };
    MathNode *inner = math_row(v_plus_half, 3);
    MathNode *parens = math_paren(inner);

    MathNode *parts[] = { E_v, eq, hbar, omega, parens };
    return math_row(parts, 5);
}

/* Topic 3: Δv = ±1  (IR selection rule) */
static MathNode *build_spectroscopy_sample(void)
{
    /* Δ and ± from OS font via symbol table */
    MathNode *delta = math_symbol("Delta");
    MathNode *v     = math_text("v");
    MathNode *dv_children[] = { delta, v };
    MathNode *delta_v = math_row(dv_children, 2);

    MathNode *eq  = math_text(" = ");
    MathNode *pm  = math_symbol("pm");
    MathNode *one = math_number("1");

    MathNode *parts[] = { delta_v, eq, pm, one };
    return math_row(parts, 4);
}

/* Topic 4: ⟨ψ|Ĥ|ψ⟩  (expectation value — sandwich) */
static MathNode *build_hydrogen_sample(void)
{
    /* ψ from OS font via symbol table */
    return math_sandwich(
        math_symbol("psi"),
        math_hat(math_text("H")),
        math_symbol("psi")
    );
}

/* Topic 5: Slater determinant concept — |ψ₁ψ₂⟩ */
static MathNode *build_multielectron_sample(void)
{
    /* ψ from OS font via symbol table */
    MathNode *psi1 = math_subscript(math_symbol("psi"), math_number("1"));
    MathNode *psi2 = math_subscript(math_symbol("psi"), math_number("2"));
    MathNode *inner[] = { psi1, psi2 };
    MathNode *content = math_row(inner, 2);
    return math_ket(content);
}

/* Dispatch table for sample equation builders */
typedef MathNode *(*SampleBuilder)(void);
static const SampleBuilder sample_builders[NUM_TOPICS] = {
    build_pib_sample,
    build_commutator_sample,
    build_oscillator_sample,
    build_spectroscopy_sample,
    build_hydrogen_sample,
    build_multielectron_sample,
};

/* -----------------------------------------------------------------------
 * topic_init — prepare a topic screen for display
 * ----------------------------------------------------------------------- */
void topic_init(TopicScreen *ts, TopicID id)
{
    ts->id        = id;
    ts->scroll_y  = 0;
    ts->content_h = 300;  /* Placeholder; real value computed from content */
}

/* -----------------------------------------------------------------------
 * topic_draw — render the topic content screen to VRAM
 *
 * Layout:
 *   ┌──────────────────────────────────┐
 *   │ ██ Topic Title ████████████████  │  header bar
 *   ├──────────────────────────────────┤
 *   │                                  │
 *   │  Description text (wrapped)...   │  text area (scrollable)
 *   │                                  │
 *   │  ┌───────────────────┐           │
 *   │  │  rendered equation │           │  math expression
 *   │  └───────────────────┘           │
 *   │                                  │
 *   │  More text / explanation...      │
 *   │                                  │
 *   └──────────────────────────────────┘
 * ----------------------------------------------------------------------- */
void topic_draw(const TopicScreen *ts)
{
    /* ---- Header ---- */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, topic_title(ts->id));

    /* ---- Hint: EXIT = back ---- */
    dtext(SCREEN_W - 90, 7, COL_HEADER_FG, "[EXIT] Back");

    /* ---- Content area (below header, offset by scroll_y) ---- */
    int cy = HEADER_H + CONTENT_PAD - ts->scroll_y;

    /* Description text */
    const char *desc = topic_description(ts->id);
    dtext(CONTENT_PAD, cy, COL_ITEM_FG, desc);
    cy += 40;  /* Approximate height of description block */

    /* Divider line */
    drect(CONTENT_PAD, cy, SCREEN_W - CONTENT_PAD, cy, C_RGB(24, 24, 24));
    cy += 10;

    /* Label */
    dtext(CONTENT_PAD, cy, COL_ITEM_FG, "Key equation:");
    cy += 20;

    /* ---- Render sample equation using the math AST ---- */
    render_pool_reset();
    MathNode *eq = sample_builders[ts->id]();
    if (eq) {
        render_layout(eq);
        render_draw(eq, CONTENT_PAD + 10, cy);
        cy += eq->layout.h + 20;
    }

    /* ---- Placeholder for future content ---- */
    dtext(CONTENT_PAD, cy, C_RGB(16, 16, 20),
          "(Full content coming soon)");

    /* Update content height for scroll bounds */
    /* (In a real implementation, we'd compute this from all content) */
}

/* -----------------------------------------------------------------------
 * topic_handle_key — process input on a topic screen
 *
 * Returns:
 *   0 = key consumed (scroll, etc.)
 *   1 = EXIT pressed (go back to menu)
 *   2 = MENU pressed (exit add-in)
 * ----------------------------------------------------------------------- */
int topic_handle_key(TopicScreen *ts, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    switch (ev.key) {

    /* Scroll up */
    case KEY_UP:
        ts->scroll_y -= 20;
        if (ts->scroll_y < 0) ts->scroll_y = 0;
        return 0;

    /* Scroll down */
    case KEY_DOWN:
        ts->scroll_y += 20;
        /* Clamp to content height minus one screen */
        if (ts->scroll_y > ts->content_h - (SCREEN_H - HEADER_H))
            ts->scroll_y = ts->content_h - (SCREEN_H - HEADER_H);
        if (ts->scroll_y < 0) ts->scroll_y = 0;
        return 0;

    /* EXIT = go back one level */
    case KEY_EXIT:
        return 1;

    /* MENU = exit add-in entirely */
    case KEY_MENU:
        return 2;

    default:
        return 0;
    }
}
