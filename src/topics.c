/* ==========================================================================
 * topics.c - Topic content, submenus, and subtopic content screens
 * ==========================================================================
 *
 * ORGANIZATION
 * ------------
 *   §1  Word-wrapping text helpers
 *   §2  Math equation builders (one per labelled equation per topic)
 *   §3  Per-topic equation arrays, keyword arrays, description strings
 *   §4  TopicContent aggregates, lookup helpers
 *   §5  Submenu implementation (topic -> Desc/Eq/Keyword chooser)
 *   §6  Topic content screen implementation (scrollable subtopic view)
 *
 * MEMORY / RENDERING CONTRACT
 * ---------------------------
 * The math-node pool is reset at the start of topic_draw() when the
 * EQUATIONS subtopic is active.  Each equation is built via its
 * MathBuilder, laid out, drawn, then overwritten in-place as the
 * scroll-measure pass progresses.  This mirrors the bump-allocator
 * lifetime convention already established in render.c.
 * ========================================================================== */

#include "topics.h"
#include "render.h"
#include "menu.h"
#include <gint/display.h>
#include <string.h>

/* =========================================================================
 * §1  WORD-WRAP TEXT HELPERS
 * =========================================================================
 *
 * The OS font on the fx-CG50 does not expose per-string measurement in a
 * convenient tier-aware way without also setting the active font.  For
 * body-text wrapping we use an approximate fixed-width model: ASCII ~ 9px,
 * multi-byte OS glyphs ~ 10px.  This matches the pre-measured widths in
 * the symbol table (render.c §2) and is accurate enough for paragraph
 * wrapping without overflowing the 396-pixel-wide screen.
 * ========================================================================= */

#define BODY_LINE_H       14
#define PARA_GAP           6
#define HEADING_H_LOCAL   18
#define BODY_CHAR_W        9
#define MULTIBYTE_W       10

/* Advance p by one glyph (1 or 2 bytes).  Returns new pointer. */
static const char *advance_glyph(const char *p)
{
    const unsigned char *u = (const unsigned char *)p;
    if (*u >= 0xE5 && *u <= 0xE7 && *(u + 1)) return (const char *)(u + 2);
    return (const char *)(u + 1);
}

/* Draw or measure a word-wrapped paragraph.
 * - `text` may contain '\n' for hard line breaks.
 * - Draws at (x, start_y) wrapping at max_w pixels.
 * - If `draw` is 0, only measures (used for scroll-height pass).
 * Returns the y coordinate AFTER the last rendered line.
 */
static int draw_wrapped(const char *text, int x, int start_y, int max_w,
                        int color, int draw)
{
    if (!text) return start_y;

    char line_buf[160];
    int line_len = 0;
    int line_w = 0;
    int cur_y = start_y;

    const char *p = text;
    const char *word_start = p;
    int word_w = 0;
    int word_len = 0;

    while (1) {
        int is_end = (*p == '\0');
        int is_space = (*p == ' ');
        int is_newline = (*p == '\n');

        if (is_space || is_newline || is_end) {
            /* Try to fit the pending word onto the current line */
            int needed = line_w + (line_len > 0 ? BODY_CHAR_W : 0) + word_w;
            if (needed > max_w && line_len > 0) {
                /* Flush current line, start new line */
                line_buf[line_len] = '\0';
                if (draw) dtext(x, cur_y, color, line_buf);
                cur_y += BODY_LINE_H;
                line_len = 0;
                line_w = 0;
            }
            /* Append space + word (or just word at line start) */
            if (line_len > 0 && line_len + 1 < (int)sizeof(line_buf)) {
                line_buf[line_len++] = ' ';
                line_w += BODY_CHAR_W;
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
                cur_y += BODY_LINE_H;
                line_len = 0;
                line_w = 0;
            }
            if (is_end) break;

            p++;
            word_start = p;
            word_w = 0;
            word_len = 0;
        } else {
            const char *next = advance_glyph(p);
            int step = (int)(next - p);
            word_w += (step == 2) ? MULTIBYTE_W : BODY_CHAR_W;
            word_len += step;
            p = next;
        }
    }
    if (line_len > 0) {
        line_buf[line_len] = '\0';
        if (draw) dtext(x, cur_y, color, line_buf);
        cur_y += BODY_LINE_H;
    }
    return cur_y;
}

/* =========================================================================
 * §2  EQUATION BUILDERS
 * =========================================================================
 * Each builder allocates MathNodes from the static pool and returns the
 * root.  render_pool_reset() is called by the content-view code before
 * each draw pass, so these builders must be callable repeatedly.
 *
 * Helpers first.
 * ========================================================================= */

/* Small inline wrapper -- clarity when building rows inline. */
static MathNode *row_of(MathNode **kids, int n) { return math_row(kids, n); }

/* ----- TOPIC 0: Particle in a Box ----- */

/* E_n = n^2 h^2 / (8 m L^2) */
static MathNode *eq_pib_energy(void)
{
    MathNode *num_k[] = {
        math_superscript(math_text("n"), math_number("2")),
        math_superscript(math_text("h"), math_number("2"))
    };
    MathNode *den_k[] = {
        math_number("8"),
        math_text("m"),
        math_superscript(math_text("L"), math_number("2"))
    };
    MathNode *frac = math_fraction(row_of(num_k, 2), row_of(den_k, 3));
    MathNode *parts[] = {
        math_subscript(math_text("E"), math_text("n")),
        math_text(" = "),
        frac
    };
    return row_of(parts, 3);
}

/* psi_n(x) = sqrt(2/L) sin(n pi x / L) */
static MathNode *eq_pib_wavefunction(void)
{
    MathNode *two_L = math_fraction(math_number("2"), math_text("L"));
    MathNode *root  = math_sqrt(two_L);

    MathNode *arg_k[] = {
        math_text("n"), math_symbol("pi"), math_text("x")
    };
    MathNode *sin_arg = math_fraction(row_of(arg_k, 3), math_text("L"));

    MathNode *parts[] = {
        math_subscript(math_symbol("psi"), math_text("n")),
        math_text("(x) = "),
        root,
        math_text(" sin"),
        math_paren(sin_arg)
    };
    return row_of(parts, 5);
}

/* ----- TOPIC 1: Commutators & Spin ----- */

/* [A_hat, B_hat] = A_hat B_hat - B_hat A_hat */
static MathNode *eq_commutator_def(void)
{
    MathNode *inner_k[] = {
        math_hat(math_text("A")),
        math_text(", "),
        math_hat(math_text("B"))
    };
    MathNode *AB_k[] = { math_hat(math_text("A")), math_hat(math_text("B")) };
    MathNode *BA_k[] = { math_hat(math_text("B")), math_hat(math_text("A")) };
    MathNode *parts[] = {
        math_text("["), row_of(inner_k, 3), math_text("]"),
        math_text(" = "),
        row_of(AB_k, 2),
        math_text(" - "),
        row_of(BA_k, 2)
    };
    return row_of(parts, 7);
}

/* Heisenberg:  Delta x  Delta p  >=  hbar/2 */
static MathNode *eq_uncertainty(void)
{
    MathNode *dx_k[] = { math_symbol("Delta"), math_text("x") };
    MathNode *dp_k[] = { math_symbol("Delta"), math_text("p") };
    MathNode *hbar2 = math_fraction(math_bar(math_text("h")), math_number("2"));
    MathNode *parts[] = {
        row_of(dx_k, 2), row_of(dp_k, 2),
        math_text(" "), math_symbol("geq"), math_text(" "),
        hbar2
    };
    return row_of(parts, 6);
}

/* ----- TOPIC 2: Harmonic Oscillator & Rotor ----- */

/* E_v = hbar omega (v + 1/2) */
static MathNode *eq_ho_energy(void)
{
    MathNode *half = math_fraction(math_number("1"), math_number("2"));
    MathNode *inner[] = { math_text("v"), math_text(" + "), half };
    MathNode *parts[] = {
        math_subscript(math_text("E"), math_text("v")),
        math_text(" = "),
        math_bar(math_text("h")),
        math_symbol("omega"),
        math_paren(row_of(inner, 3))
    };
    return row_of(parts, 5);
}

/* E_J = hbar^2 J (J+1) / (2 I) */
static MathNode *eq_rotor_energy(void)
{
    MathNode *hb2 = math_superscript(math_bar(math_text("h")),
                                     math_number("2"));
    MathNode *jp1_k[] = { math_text("J"), math_text(" + "), math_number("1") };
    MathNode *num_k[] = { hb2, math_text("J"), math_paren(row_of(jp1_k, 3)) };
    MathNode *den_k[] = { math_number("2"), math_text("I") };
    MathNode *parts[] = {
        math_subscript(math_text("E"), math_text("J")),
        math_text(" = "),
        math_fraction(row_of(num_k, 3), row_of(den_k, 2))
    };
    return row_of(parts, 3);
}

/* ----- TOPIC 3: Diatomic Spectroscopy (FULL LECTURE 8 CONTENT) ----- */

/* mu = m1 m2 / (m1 + m2) */
static MathNode *eq_reduced_mass(void)
{
    MathNode *num_k[] = {
        math_subscript(math_text("m"), math_number("1")),
        math_subscript(math_text("m"), math_number("2"))
    };
    MathNode *den_k[] = {
        math_subscript(math_text("m"), math_number("1")),
        math_text(" + "),
        math_subscript(math_text("m"), math_number("2"))
    };
    MathNode *parts[] = {
        math_symbol("mu"), math_text(" = "),
        math_fraction(row_of(num_k, 2), row_of(den_k, 3))
    };
    return row_of(parts, 3);
}

/* I = mu r^2 */
static MathNode *eq_moment_of_inertia(void)
{
    MathNode *parts[] = {
        math_text("I"), math_text(" = "),
        math_symbol("mu"),
        math_superscript(math_text("r"), math_number("2"))
    };
    return row_of(parts, 4);
}

/* B = h / (8 pi^2 I) */
static MathNode *eq_rotational_constant(void)
{
    MathNode *pi_sq = math_superscript(math_symbol("pi"), math_number("2"));
    MathNode *den_k[] = { math_number("8"), pi_sq, math_text("I") };
    MathNode *parts[] = {
        math_text("B"), math_text(" = "),
        math_fraction(math_text("h"), row_of(den_k, 3))
    };
    return row_of(parts, 3);
}

/* nu_tilde = 1/lambda = nu/c */
static MathNode *eq_wavenumber(void)
{
    MathNode *parts[] = {
        math_bar(math_symbol("nu")), math_text(" = "),
        math_fraction(math_number("1"), math_symbol("lambda")),
        math_text(" = "),
        math_fraction(math_symbol("nu"), math_text("c"))
    };
    return row_of(parts, 5);
}

/* Selection rule:  Delta n = +/- 1 */
static MathNode *eq_selection_rule(void)
{
    MathNode *dn_k[] = { math_symbol("Delta"), math_text("n") };
    MathNode *parts[] = {
        row_of(dn_k, 2), math_text(" = "),
        math_symbol("pm"), math_number("1")
    };
    return row_of(parts, 4);
}

/* ----- TOPIC 4: Hydrogen Atom (LECTURE 9) ----- */

/* H_hat = T_hat + V_hat */
static MathNode *eq_hamiltonian_total(void)
{
    MathNode *parts[] = {
        math_hat(math_text("H")), math_text(" = "),
        math_hat(math_text("T")), math_text(" + "),
        math_hat(math_text("V"))
    };
    return row_of(parts, 5);
}

/* V_hat = -e^2 / (4 pi eps0 rho) */
static MathNode *eq_hydrogen_potential(void)
{
    MathNode *e_sq = math_superscript(math_text("e"), math_number("2"));
    MathNode *den_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_symbol("rho")
    };
    MathNode *frac = math_fraction(e_sq, row_of(den_k, 4));
    MathNode *parts[] = {
        math_hat(math_text("V")), math_text(" = -"), frac
    };
    return row_of(parts, 3);
}

/* a_0 = (4 pi eps0 hbar^2) / (m_e e^2) */
static MathNode *eq_bohr_radius(void)
{
    MathNode *hb2 = math_superscript(math_bar(math_text("h")),
                                     math_number("2"));
    MathNode *num_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        hb2
    };
    MathNode *e_sq = math_superscript(math_text("e"), math_number("2"));
    MathNode *den_k[] = {
        math_subscript(math_text("m"), math_text("e")),
        e_sq
    };
    MathNode *parts[] = {
        math_subscript(math_text("a"), math_number("0")),
        math_text(" = "),
        math_fraction(row_of(num_k, 4), row_of(den_k, 2))
    };
    return row_of(parts, 3);
}

/* ----- TOPIC 5: Many-Electron Atoms ----- */

/* Psi(r1,r2,...,rn) = psi1(r1) psi2(r2) ... psin(rn) */
static MathNode *eq_separable_wavefunction(void)
{
    /* LHS: Psi(r1, r2, ..., rn) */
    MathNode *lhs_args_k[] = {
        math_subscript(math_text("r"), math_number("1")),
        math_text(", "),
        math_subscript(math_text("r"), math_number("2")),
        math_text(", ... , "),
        math_subscript(math_text("r"), math_text("n"))
    };
    MathNode *lhs_k[] = {
        math_symbol("Psi"),
        math_paren(row_of(lhs_args_k, 5))
    };
    MathNode *lhs = row_of(lhs_k, 2);

    /* RHS: psi1(r1) psi2(r2) ... psin(rn) */
    MathNode *p1_k[] = {
        math_subscript(math_symbol("psi"), math_number("1")),
        math_paren(math_subscript(math_text("r"), math_number("1")))
    };
    MathNode *p2_k[] = {
        math_subscript(math_symbol("psi"), math_number("2")),
        math_paren(math_subscript(math_text("r"), math_number("2")))
    };
    MathNode *pn_k[] = {
        math_subscript(math_symbol("psi"), math_text("n")),
        math_paren(math_subscript(math_text("r"), math_text("n")))
    };

    MathNode *parts[] = {
        lhs, math_text(" = "),
        row_of(p1_k, 2),
        row_of(p2_k, 2),
        math_text(" ... "),
        row_of(pn_k, 2)
    };
    return row_of(parts, 6);
}

/* Effective 1-e Hamiltonian: H_n = -hbar^2/(2m) grad_n^2 + V_eff,n(r_n) */
static MathNode *eq_effective_hamiltonian(void)
{
    MathNode *hb2 = math_superscript(math_bar(math_text("h")),
                                     math_number("2"));
    MathNode *den_k[] = { math_number("2"), math_text("m") };
    /* grad_n^2 rendered as (nabla_n)^2 using the "nabla" symbol fallback */
    MathNode *grad_n_sq = math_superscript(
        math_subscript(math_symbol("nabla"), math_text("n")),
        math_number("2"));
    MathNode *V_eff_args_k[] = {
        math_text("eff, n")
    };
    MathNode *V_eff = math_subscript(math_text("V"), row_of(V_eff_args_k, 1));

    MathNode *parts[] = {
        math_subscript(math_hat(math_text("H")), math_text("n")),
        math_text(" = -"),
        math_fraction(hb2, row_of(den_k, 2)),
        grad_n_sq,
        math_text(" + "),
        V_eff,
        math_paren(math_subscript(math_text("r"), math_text("n")))
    };
    return row_of(parts, 7);
}

/* Helium Hamiltonian -- displayed in compact form (5 terms) */
static MathNode *eq_helium_hamiltonian(void)
{
    MathNode *hb2 = math_superscript(math_bar(math_text("h")),
                                     math_number("2"));
    MathNode *den_2m_k[] = {
        math_number("2"),
        math_subscript(math_text("m"), math_text("e"))
    };
    MathNode *kin_factor = math_fraction(hb2, row_of(den_2m_k, 2));
    MathNode *grad1_sq = math_superscript(
        math_subscript(math_symbol("nabla"), math_number("1")),
        math_number("2"));
    MathNode *grad2_sq = math_superscript(
        math_subscript(math_symbol("nabla"), math_number("2")),
        math_number("2"));

    /* 2e^2 / (4 pi eps0 r1) -- simplified with text for compactness */
    MathNode *e_sq = math_superscript(math_text("e"), math_number("2"));
    MathNode *atr_num_k[] = { math_number("2"), e_sq };
    MathNode *atr_den1_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_subscript(math_text("r"), math_number("1"))
    };
    MathNode *atr1 = math_fraction(row_of(atr_num_k, 2),
                                   row_of(atr_den1_k, 4));

    MathNode *e_sq_2 = math_superscript(math_text("e"), math_number("2"));
    MathNode *r12_k[] = { math_number("1"), math_number("2") };
    MathNode *rep_den_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_subscript(math_text("r"), row_of(r12_k, 2))
    };
    MathNode *rep = math_fraction(e_sq_2, row_of(rep_den_k, 4));

    MathNode *parts[] = {
        math_hat(math_text("H")), math_text(" = -"),
        kin_factor, grad1_sq,
        math_text(" - "),
        kin_factor, grad2_sq,
        math_text(" - "), atr1,
        math_text(" - ... + "), rep
    };
    return row_of(parts, 11);
}

/* -------------------------------------------------------------------------
 * NEW (HW 9-14) - Chapter 20: Hydrogen - 1s orbital / radial probability
 * ------------------------------------------------------------------------- */

/* psi_1s = 1 / sqrt(pi a0^3) * e^(-r/a0) */
static MathNode *eq_1s_orbital(void)
{
    MathNode *a0_cubed = math_superscript(
        math_subscript(math_text("a"), math_number("0")),
        math_number("3"));
    MathNode *pi_a0_k[] = { math_symbol("pi"), a0_cubed };
    MathNode *root = math_sqrt(row_of(pi_a0_k, 2));
    MathNode *prefactor = math_fraction(math_number("1"), root);

    /* e^(-r/a0) */
    MathNode *exp_arg_k[] = {
        math_text("-"),
        math_fraction(math_text("r"),
                      math_subscript(math_text("a"), math_number("0")))
    };
    MathNode *exp_part = math_superscript(math_text("e"),
                                          row_of(exp_arg_k, 2));

    MathNode *parts[] = {
        math_subscript(math_symbol("psi"), math_text("1s")),
        math_text(" = "),
        prefactor,
        math_text(" "),
        exp_part
    };
    return row_of(parts, 5);
}

/* Radial probability:  P_1s(r) = 4 r^2 / a0^3 * e^(-2r/a0)  (proportional form) */
static MathNode *eq_radial_probability(void)
{
    /* 4 r^2 / a0^3 */
    MathNode *a0_cubed = math_superscript(
        math_subscript(math_text("a"), math_number("0")),
        math_number("3"));
    MathNode *num_k[] = {
        math_number("4"),
        math_superscript(math_text("r"), math_number("2"))
    };
    MathNode *prefactor = math_fraction(row_of(num_k, 2), a0_cubed);

    /* e^(-2r/a0) */
    MathNode *exp_arg_k[] = {
        math_text("-"),
        math_fraction(
            row_of((MathNode*[]){ math_number("2"), math_text("r") }, 2),
            math_subscript(math_text("a"), math_number("0")))
    };
    MathNode *exp_part = math_superscript(math_text("e"),
                                          row_of(exp_arg_k, 2));

    MathNode *parts[] = {
        math_subscript(math_text("P"), math_text("1s")),
        math_text("(r) = "),
        prefactor,
        math_text(" "),
        exp_part
    };
    return row_of(parts, 5);
}

/* Most probable radius condition:  d/dr P_1s(r) = 0  =>  r_mp = a0 */
static MathNode *eq_most_probable_radius(void)
{
    /* d/dr ( P_1s(r) ) = 0  =>  r = a0 */
    MathNode *ddr = math_fraction(math_text("d"),
                                  math_text("dr"));
    MathNode *P_of_r_k[] = {
        math_subscript(math_text("P"), math_text("1s")),
        math_text("(r)")
    };
    MathNode *lhs_k[] = { ddr, math_paren(row_of(P_of_r_k, 2)) };
    MathNode *parts[] = {
        row_of(lhs_k, 2),
        math_text(" = 0  =>  "),
        math_subscript(math_text("r"), math_text("mp")),
        math_text(" = "),
        math_subscript(math_text("a"), math_number("0"))
    };
    return row_of(parts, 5);
}

/* -------------------------------------------------------------------------
 * NEW (HW 9-14) - Chapter 21: Slater Determinant (N-electron antisymm WF)
 * ------------------------------------------------------------------------- */

/* Psi(1,2,...,N) = 1/sqrt(N!) det|...|  -- displayed in condensed form */
static MathNode *eq_slater_determinant(void)
{
    /* 1 / sqrt(N!) */
    MathNode *N_fact_k[] = { math_text("N"), math_text("!") };
    MathNode *prefactor = math_fraction(
        math_number("1"),
        math_sqrt(row_of(N_fact_k, 2)));

    /* det|chi_1(1)  chi_2(1) ... chi_N(1) ; ... ; chi_1(N) ... chi_N(N)| */
    MathNode *det_inner_k[] = {
        math_subscript(math_symbol("chi"), math_number("1")),
        math_paren(math_number("1")),
        math_text(" ... "),
        math_subscript(math_symbol("chi"), math_text("N")),
        math_paren(math_text("N"))
    };
    MathNode *det_label_k[] = {
        math_text("det|"),
        row_of(det_inner_k, 5),
        math_text("|")
    };

    /* Psi(1, 2, ..., N) */
    MathNode *lhs_args_k[] = {
        math_number("1"), math_text(", "),
        math_number("2"), math_text(", ... , "),
        math_text("N")
    };
    MathNode *lhs_k[] = {
        math_symbol("Psi"),
        math_paren(row_of(lhs_args_k, 5))
    };

    MathNode *parts[] = {
        row_of(lhs_k, 2),
        math_text(" = "),
        prefactor,
        math_text(" "),
        row_of(det_label_k, 3)
    };
    return row_of(parts, 5);
}

/* -------------------------------------------------------------------------
 * NEW (HW 9-14) - Chapter 23: Molecular Orbitals (LCAO-MO)
 * ------------------------------------------------------------------------- */

/* Multi-electron / multi-nuclei Hamiltonian (condensed, 4 terms) */
static MathNode *eq_molecular_hamiltonian(void)
{
    /* -hbar^2/(2 m_e) Sum(nabla_i^2) */
    MathNode *hb2 = math_superscript(math_bar(math_text("h")),
                                     math_number("2"));
    MathNode *den_2m_k[] = {
        math_number("2"),
        math_subscript(math_text("m"), math_text("e"))
    };
    MathNode *kin_factor = math_fraction(hb2, row_of(den_2m_k, 2));
    MathNode *grad_i_sq = math_superscript(
        math_subscript(math_symbol("nabla"), math_text("i")),
        math_number("2"));
    MathNode *sum_kin = math_summation(math_text("i"), NULL, grad_i_sq);

    /* - Sum_{i,A} Z_A e^2 / (4 pi eps0 r_iA) */
    MathNode *e_sq_a = math_superscript(math_text("e"), math_number("2"));
    MathNode *attr_num_k[] = {
        math_subscript(math_text("Z"), math_text("A")),
        e_sq_a
    };
    MathNode *attr_den_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_subscript(math_text("r"), math_text("iA"))
    };
    MathNode *attr_term = math_fraction(row_of(attr_num_k, 2),
                                        row_of(attr_den_k, 4));
    MathNode *sum_attr = math_summation(math_text("i,A"), NULL, attr_term);

    /* + Sum_{i<j} e^2 / (4 pi eps0 r_ij) */
    MathNode *e_sq_b = math_superscript(math_text("e"), math_number("2"));
    MathNode *rep_den_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_subscript(math_text("r"), math_text("ij"))
    };
    MathNode *rep_term = math_fraction(e_sq_b, row_of(rep_den_k, 4));
    MathNode *sum_rep = math_summation(math_text("i<j"), NULL, rep_term);

    /* + Z_A Z_B e^2 / (4 pi eps0 R_AB) */
    MathNode *e_sq_c = math_superscript(math_text("e"), math_number("2"));
    MathNode *nn_num_k[] = {
        math_subscript(math_text("Z"), math_text("A")),
        math_subscript(math_text("Z"), math_text("B")),
        e_sq_c
    };
    MathNode *nn_den_k[] = {
        math_number("4"), math_symbol("pi"),
        math_subscript(math_symbol("epsilon"), math_number("0")),
        math_subscript(math_text("R"), math_text("AB"))
    };
    MathNode *nn_term = math_fraction(row_of(nn_num_k, 3),
                                      row_of(nn_den_k, 4));

    MathNode *parts[] = {
        math_hat(math_text("H")), math_text(" = -"),
        kin_factor, sum_kin,
        math_text(" - "), sum_attr,
        math_text(" + "), sum_rep,
        math_text(" + "), nn_term
    };
    return row_of(parts, 10);
}

/* Overlap integral: S_12 = <phi_1s(1) | phi_1s(2)> */
static MathNode *eq_overlap_integral(void)
{
    MathNode *phi_1 = row_of((MathNode*[]){
        math_subscript(math_symbol("phi"), math_text("1s")),
        math_paren(math_number("1"))
    }, 2);
    MathNode *phi_2 = row_of((MathNode*[]){
        math_subscript(math_symbol("phi"), math_text("1s")),
        math_paren(math_number("2"))
    }, 2);

    MathNode *S12_k[] = { math_number("1"), math_number("2") };
    MathNode *parts[] = {
        math_subscript(math_text("S"), row_of(S12_k, 2)),
        math_text(" = "),
        math_braket(phi_1, phi_2)
    };
    return row_of(parts, 3);
}

/* c_g = 1 / sqrt(2 (1 + S_12))   (bonding / gerade coefficient) */
static MathNode *eq_cg_coefficient(void)
{
    MathNode *S12_k[] = { math_number("1"), math_number("2") };
    MathNode *inside_paren_k[] = {
        math_number("1"), math_text(" + "),
        math_subscript(math_text("S"), row_of(S12_k, 2))
    };
    MathNode *inside_root_k[] = {
        math_number("2"),
        math_paren(row_of(inside_paren_k, 3))
    };
    MathNode *parts[] = {
        math_subscript(math_text("c"), math_text("g")),
        math_text(" = "),
        math_fraction(math_number("1"),
                      math_sqrt(row_of(inside_root_k, 2)))
    };
    return row_of(parts, 3);
}

/* c_u = 1 / sqrt(2 (1 - S_12))  (antibonding / ungerade coefficient) */
static MathNode *eq_cu_coefficient(void)
{
    MathNode *S12_k[] = { math_number("1"), math_number("2") };
    MathNode *inside_paren_k[] = {
        math_number("1"), math_text(" - "),
        math_subscript(math_text("S"), row_of(S12_k, 2))
    };
    MathNode *inside_root_k[] = {
        math_number("2"),
        math_paren(row_of(inside_paren_k, 3))
    };
    MathNode *parts[] = {
        math_subscript(math_text("c"), math_text("u")),
        math_text(" = "),
        math_fraction(math_number("1"),
                      math_sqrt(row_of(inside_root_k, 2)))
    };
    return row_of(parts, 3);
}

/* -------------------------------------------------------------------------
 * NEW (HW 9-14) - Chapter 24: sp Hybrid Orbitals
 * ------------------------------------------------------------------------- */

/* psi_sp,1 = c1 phi_2s + c2 phi_2px */
static MathNode *eq_sp_hybrid_def(void)
{
    MathNode *psi_label = math_subscript(math_symbol("psi"),
                                         math_text("sp,1"));
    MathNode *c1_phi2s_k[] = {
        math_subscript(math_text("c"), math_number("1")),
        math_subscript(math_symbol("phi"), math_text("2s"))
    };
    MathNode *c2_phi2p_k[] = {
        math_subscript(math_text("c"), math_number("2")),
        math_subscript(math_symbol("phi"), math_text("2px"))
    };
    MathNode *parts[] = {
        psi_label, math_text(" = "),
        row_of(c1_phi2s_k, 2),
        math_text(" + "),
        row_of(c2_phi2p_k, 2)
    };
    return row_of(parts, 5);
}

/* Normalization:  c1^2 + c2^2 = 1 */
static MathNode *eq_sp_normalization(void)
{
    MathNode *c1_sq = math_superscript(
        math_subscript(math_text("c"), math_number("1")),
        math_number("2"));
    MathNode *c2_sq = math_superscript(
        math_subscript(math_text("c"), math_number("2")),
        math_number("2"));
    MathNode *parts[] = {
        c1_sq, math_text(" + "), c2_sq,
        math_text(" = 1")
    };
    return row_of(parts, 4);
}

/* Orthogonality:  c1 c5 + c2 c6 = 0 */
static MathNode *eq_sp_orthogonality(void)
{
    MathNode *c1c5_k[] = {
        math_subscript(math_text("c"), math_number("1")),
        math_subscript(math_text("c"), math_number("5"))
    };
    MathNode *c2c6_k[] = {
        math_subscript(math_text("c"), math_number("2")),
        math_subscript(math_text("c"), math_number("6"))
    };
    MathNode *parts[] = {
        row_of(c1c5_k, 2),
        math_text(" + "),
        row_of(c2c6_k, 2),
        math_text(" = 0")
    };
    return row_of(parts, 4);
}

/* Equal contribution rule:  c1^2 + c5^2 = 1   (and  c2^2 + c6^2 = 1) */
static MathNode *eq_sp_equal_contribution(void)
{
    MathNode *c1_sq = math_superscript(
        math_subscript(math_text("c"), math_number("1")),
        math_number("2"));
    MathNode *c5_sq = math_superscript(
        math_subscript(math_text("c"), math_number("5")),
        math_number("2"));
    MathNode *parts[] = {
        c1_sq, math_text(" + "), c5_sq,
        math_text(" = 1")
    };
    return row_of(parts, 4);
}

/* -------------------------------------------------------------------------
 * NEW (HW 9-14) - Boltzmann population ratio
 * ------------------------------------------------------------------------- */

/* p_LUMO / p_HOMO = e^(-Delta E / kT) */
static MathNode *eq_boltzmann_ratio(void)
{
    /* Exponent: -Delta E / kT */
    MathNode *DeltaE_k[] = { math_symbol("Delta"), math_text("E") };
    MathNode *exp_num_k[] = {
        math_text("-"), row_of(DeltaE_k, 2)
    };
    MathNode *exp_frac = math_fraction(row_of(exp_num_k, 2),
                                       math_text("kT"));

    MathNode *ratio = math_fraction(
        math_subscript(math_text("p"), math_text("LUMO")),
        math_subscript(math_text("p"), math_text("HOMO")));

    MathNode *parts[] = {
        ratio, math_text(" = "),
        math_superscript(math_text("e"), exp_frac)
    };
    return row_of(parts, 3);
}

/* =========================================================================
 * §3  TOPIC CONTENT ARRAYS
 * ========================================================================= */

/* ---------- TOPIC 0: PARTICLE IN A BOX ---------- */

static const EquationEntry eqs_pib[] = {
    { "Energy Levels",
      eq_pib_energy,
      "E_n : energy of level n\n"
      "n   : quantum number (1, 2, 3, ...)\n"
      "h   : Planck's constant\n"
      "m   : particle mass\n"
      "L   : box length" },
    { "Wavefunction",
      eq_pib_wavefunction,
      "psi_n(x) : normalised wavefunction for level n\n"
      "n        : quantum number\n"
      "L        : box length\n"
      "x        : position coordinate" },
    { "Boltzmann Population Ratio",
      eq_boltzmann_ratio,
      "p_LUMO / p_HOMO : thermal-population ratio\n"
      "Delta E         : E_LUMO - E_HOMO (e.g. E_4 - E_3)\n"
      "k               : Boltzmann constant (1.38e-23 J/K)\n"
      "T               : absolute temperature (K)\n"
      "Apply with E_n from a 1D box for HOMO/LUMO energies." },
};

static const KeywordEntry kws_pib[] = {
    { "Quantization",
      "Energy is restricted to discrete levels because the wavefunction "
      "must vanish at the walls of the box." },
    { "Zero-Point Energy",
      "The lowest allowed energy (n=1) is non-zero: the particle cannot "
      "be completely at rest." },
    { "Nodes",
      "The n-th wavefunction has (n-1) interior nodes." },
    { "Tunneling",
      "For finite-wall wells the wavefunction penetrates classically "
      "forbidden regions, enabling quantum tunneling." },
    { "Band Theory",
      "Extending the box model to periodic potentials in solids leads "
      "to energy bands and band gaps." },
    { "HOMO / LUMO",
      "Highest Occupied and Lowest Unoccupied Molecular Orbital. In a "
      "1D-box model of a conjugated pi-system, HOMO corresponds to the "
      "highest filled level n, LUMO to level n+1." },
    { "Boltzmann Distribution",
      "At temperature T, the probability of occupying a state of energy "
      "E is proportional to exp(-E/kT). The ratio of two level "
      "populations collapses to exp(-Delta E / kT)." },
    { "Conjugation Length (L)",
      "The effective length of the conjugated pi-system; used as the "
      "box length when modelling HOMO/LUMO energies (e.g. L = 720 pm)." },
};

/* ---------- TOPIC 1: COMMUTATORS & SPIN ---------- */

static const EquationEntry eqs_commutators[] = {
    { "Commutator Definition",
      eq_commutator_def,
      "A_hat, B_hat : quantum operators\n"
      "[A, B]       : commutator\n"
      "If [A, B] = 0, observables are simultaneously knowable." },
    { "Heisenberg Uncertainty",
      eq_uncertainty,
      "Delta x : uncertainty in position\n"
      "Delta p : uncertainty in momentum\n"
      "hbar    : reduced Planck's constant" },
};

static const KeywordEntry kws_commutators[] = {
    { "Commutator",
      "[A, B] = AB - BA.  Measures how far two operators are from "
      "commuting." },
    { "Angular Momentum Operator",
      "Vector operator L_hat = r x p_hat whose components do not all "
      "commute; only L^2 and one component can be known simultaneously." },
    { "Spin",
      "Intrinsic angular momentum with no classical analogue.  For "
      "electrons, spin-1/2 gives two basis states called alpha and beta." },
    { "Stern-Gerlach Experiment",
      "Historical experiment that demonstrated electron spin by splitting "
      "a silver-atom beam with an inhomogeneous magnetic field." },
    { "Uncertainty Principle",
      "Non-commuting observables obey a product lower bound: the more "
      "sharply one is known, the less sharply the other can be." },
};

/* ---------- TOPIC 2: HARMONIC OSCILLATOR & ROTOR ---------- */

static const EquationEntry eqs_oscillator[] = {
    { "Oscillator Energy",
      eq_ho_energy,
      "E_v   : energy of vibrational level v\n"
      "v     : vibrational quantum number (0, 1, 2, ...)\n"
      "hbar  : reduced Planck's constant\n"
      "omega : angular frequency" },
    { "Rigid Rotor Energy",
      eq_rotor_energy,
      "E_J   : rotational energy of level J\n"
      "hbar  : reduced Planck's constant\n"
      "J     : rotational quantum number (0, 1, 2, ...)\n"
      "I     : moment of inertia" },
};

static const KeywordEntry kws_oscillator[] = {
    { "Harmonic Oscillator",
      "Quantum model of a spring-like bond: equally spaced levels "
      "separated by hbar*omega." },
    { "Hermite Polynomials",
      "The polynomial factors in the exact oscillator wavefunctions." },
    { "Rigid Rotor",
      "Idealised model of a rotating diatomic: two masses at fixed "
      "separation, with quantized angular momentum." },
    { "Moment of Inertia (I)",
      "The rotational analogue of mass.  For a rotor: I = mu * r^2." },
    { "Zero-Point Vibration",
      "The v=0 state still has energy (1/2) hbar omega; a molecule "
      "never stops vibrating entirely." },
};

/* ---------- TOPIC 3: DIATOMIC SPECTROSCOPY (LECTURE 8 - FULL) ---------- */

static const char *desc_spectroscopy =
    "Photons have both electric and magnetic field components. "
    "Molecules interact specifically with the electric field in IR "
    "spectroscopy, and with the magnetic field in NMR.\n\n"
    "To absorb IR radiation a molecule must have a dynamic electric "
    "transition dipole: the dipole moment must CHANGE during the "
    "vibration. A permanent static dipole is not required, but "
    "homonuclear diatomics such as H2, N2 and O2 have zero dipole "
    "change and therefore do not absorb IR light.\n\n"
    "At room temperature (298 K) only the vibrational ground state is "
    "significantly populated, whereas many rotational levels "
    "(J = 0 through about 12) are populated at once.\n\n"
    "IR absorption stimulates simultaneous rotational AND vibrational "
    "transitions. The resulting spectrum shows two branches: the "
    "R-branch (Delta J = +1) on the high-frequency side and the "
    "P-branch (Delta J = -1) on the low-frequency side. The branch "
    "spacing is proportional to the rotational constant B, so measuring "
    "either branch gives the bond length directly.";

static const EquationEntry eqs_spectroscopy[] = {
    { "Reduced Mass",
      eq_reduced_mass,
      "mu    : reduced mass (equivalent 1-body mass for a 2-body system)\n"
      "m1,m2 : masses of the two individual bodies" },
    { "Moment of Inertia",
      eq_moment_of_inertia,
      "I  : moment of inertia (angular-momentum inertia)\n"
      "mu : reduced mass\n"
      "r  : bond length between the two bodies" },
    { "Rotational Constant",
      eq_rotational_constant,
      "B : rotational constant\n"
      "h : Planck's constant\n"
      "I : moment of inertia" },
    { "Wavenumber",
      eq_wavenumber,
      "nu_tilde : wavenumber (cm^-1), common FTIR unit\n"
      "lambda   : wavelength\n"
      "nu       : frequency\n"
      "c        : speed of light" },
    { "IR Selection Rule",
      eq_selection_rule,
      "Delta n : change in vibrational quantum number\n"
      "+1 for absorption, -1 for emission\n"
      "Also required: nonzero transition dipole." },
};

static const KeywordEntry kws_spectroscopy[] = {
    { "IR Spectroscopy (FTIR)",
      "Spectroscopy that measures absorption of infrared light to drive "
      "vibrational (and rotational) transitions." },
    { "Reduced Mass (mu)",
      "Equivalent 1-body mass used to describe a 2-body system." },
    { "Moment of Inertia (I)",
      "Measure of rotational inertia.  For a diatomic: I = mu * r^2." },
    { "Rotational Constant (B)",
      "Constant related to the slope of P/R branches; proportional to "
      "1/I, so it reveals the bond length directly." },
    { "Wavenumber (nu_tilde)",
      "Waves per unit distance, typically cm^-1; the standard unit in "
      "FTIR." },
    { "Selection Rules",
      "Quantum conditions that must be met for a transition: nonzero "
      "transition dipole AND Delta n = +/- 1 for vibration." },
    { "R-Branch",
      "Rovibrational transitions with Delta J = +1; appears on the "
      "high-frequency side of the band." },
    { "P-Branch",
      "Rovibrational transitions with Delta J = -1; appears on the "
      "low-frequency side of the band." },
    { "Transition Dipole",
      "Integral <m|mu_hat|n> between initial and final states; must be "
      "nonzero for the transition to occur." },
    { "Dynamic Dipole",
      "A dipole moment that changes during the vibration.  Required for "
      "IR absorption." },
};

/* ---------- TOPIC 4: HYDROGEN ATOM (LECTURE 9) ---------- */

static const char *desc_hydrogen =
    "The Bohr model reproduces hydrogen energies but not wavefunctions. "
    "Solving the Schrodinger equation in spherical polar coordinates "
    "(rho, theta, phi) yields all orbitals, their degeneracies, and the "
    "full angular and radial structure.\n\n"
    "The Hamiltonian splits into kinetic and potential parts. Changing "
    "to spherical polar coordinates separates the problem into three "
    "independent 1D differential equations.\n\n"
    "The wavefunction factorises as psi(rho, theta, phi) = "
    "R_nl(rho) Y_lm(theta, phi). The angular part Y_lm consists of "
    "spherical harmonics which directly give the geometric shapes of "
    "s, p and d orbitals. The radial part R_nl balances an attractive "
    "electrostatic potential against a repulsive centripetal term; its "
    "solutions involve Laguerre polynomials and the Bohr radius a0 as "
    "the natural length scale.\n\n"
    "The 1s ground state is spherically symmetric: psi_1s = "
    "(1 / sqrt(pi a0^3)) exp(-r/a0). The radial probability of finding "
    "the electron in a shell of radius r and thickness dr is "
    "P_1s(r) dr = psi* psi r^2 dr, where the r^2 Jacobian comes from "
    "the spherical volume element. Setting d/dr P_1s(r) = 0 yields the "
    "most probable radius r_mp = a0: the electron is most likely found "
    "at exactly the Bohr radius from the nucleus.";

static const EquationEntry eqs_hydrogen[] = {
    { "Total Hamiltonian",
      eq_hamiltonian_total,
      "H_hat : total Hamiltonian (total energy operator)\n"
      "T_hat : kinetic energy operator\n"
      "V_hat : potential energy operator" },
    { "Coulomb Potential",
      eq_hydrogen_potential,
      "V_hat   : potential energy operator\n"
      "e       : elementary charge\n"
      "epsilon0: vacuum permittivity\n"
      "rho     : electron-nucleus distance" },
    { "Bohr Radius",
      eq_bohr_radius,
      "a0       : Bohr radius (~= 52.9 pm)\n"
      "epsilon0 : vacuum permittivity\n"
      "hbar     : reduced Planck's constant\n"
      "m_e      : electron mass\n"
      "e        : elementary charge" },
    { "1s Orbital Wavefunction",
      eq_1s_orbital,
      "psi_1s : hydrogen ground-state wavefunction\n"
      "a0     : Bohr radius (~= 52.9 pm)\n"
      "r      : electron distance from the nucleus\n"
      "The 1s orbital is spherically symmetric and nodeless." },
    { "Radial Probability P_1s(r)",
      eq_radial_probability,
      "P_1s(r) : probability of finding electron in a spherical\n"
      "          shell of radius r (thickness dr)\n"
      "The r^2 Jacobian is essential: dV = 4 pi r^2 dr.\n"
      "Full form: P_1s(r) = integral of psi* psi * r^2 dr." },
    { "Most Probable Radius",
      eq_most_probable_radius,
      "Set d/dr of P_1s(r) equal to zero and solve for r.\n"
      "For the hydrogen 1s orbital the result is r_mp = a0,\n"
      "so the electron is most likely found at exactly the\n"
      "Bohr radius from the nucleus." },
};

static const KeywordEntry kws_hydrogen[] = {
    { "Spherical Polar Coordinates",
      "3D coordinate system (rho, theta, phi) used to simplify the "
      "Schrodinger equation for spherical atoms." },
    { "Separable Wavefunction",
      "Mathematical trick of writing psi as a product of radial and "
      "angular factors; allows each to be solved independently." },
    { "Spherical Harmonics (Y_lm)",
      "Angular functions that solve the angular part of the hydrogen "
      "problem and define s, p, d orbital shapes." },
    { "Effective Potential",
      "Sum of attractive Coulomb and repulsive centripetal terms in "
      "the radial Schrodinger equation." },
    { "Laguerre Polynomials",
      "Polynomial solutions used to build the radial wavefunction "
      "R_nl(rho)." },
    { "Bohr Radius (a0)",
      "Natural length scale of hydrogen; the most probable proton-to-"
      "electron distance in the ground state (~= 52.9 pm)." },
    { "Principal Quantum Number (n)",
      "Indexes the shell (1, 2, 3, ...) and sets the overall energy." },
    { "Angular Momentum Number (l)",
      "Indexes orbital shape within a shell: 0=s, 1=p, 2=d, ..." },
    { "Radial Probability P(r) dr",
      "Probability of finding the electron in a spherical shell of "
      "radius r and thickness dr.  Equals psi* psi r^2 dr -- the r^2 "
      "Jacobian comes from the spherical volume element." },
    { "Most Probable Radius (r_mp)",
      "Radius at which the radial probability P(r) is maximal.  Found "
      "by setting dP/dr = 0.  For the 1s orbital, r_mp = a0." },
};

/* ---------- TOPIC 5: MANY-ELECTRON ATOMS ---------- */

static const char *desc_multielectron =
    "The Schrodinger equation cannot be solved analytically for atoms "
    "with more than one electron: electrons are not point particles and "
    "their instantaneous positions cannot be perfectly modelled due to "
    "mutual electron-electron repulsion.\n\n"
    "As a first approximation, the many-electron wavefunction is treated "
    "as a simple product of separable 1-electron wavefunctions. This "
    "would be exact only if electron-electron repulsion were absent.\n\n"
    "The Variation Theorem states that the true energy can be "
    "approached from above using a trial wavefunction Phi that obeys "
    "the correct boundary conditions. In the Self-Consistent Field "
    "(Hartree-Fock) approximation, each electron feels an averaged "
    "effective field produced by all the others, yielding a one-electron "
    "Hamiltonian that is solved iteratively until self-consistency.\n\n"
    "Antisymmetry under electron exchange is enforced by a Slater "
    "determinant of spin-orbitals: rows label electrons, columns label "
    "spin-orbitals (e.g. 1s-alpha, 1s-beta, 2s-alpha, 2s-beta for Be), "
    "and the normalisation prefactor is 1/sqrt(N!).\n\n"
    "For molecules the Hamiltonian adds nucleus-nucleus repulsion and "
    "two-centre electron-nucleus terms. Molecular orbitals are built "
    "as linear combinations of atomic orbitals (LCAO-MO). The overlap "
    "integral S_12 between two 1s orbitals sets the normalisation "
    "constants for the symmetric (bonding, gerade) MO, "
    "c_g = 1/sqrt(2(1 + S_12)), and the antisymmetric (antibonding, "
    "ungerade) MO, c_u = 1/sqrt(2(1 - S_12)).\n\n"
    "Hybrid orbitals (sp, sp2, sp3) are built on a single atom from "
    "atomic orbitals subject to three constraints: normalisation "
    "(|c|^2 = 1), orthogonality (<psi_i|psi_j> = 0 for i != j), and "
    "equal contribution (each atomic orbital contributes equally to "
    "the full hybrid set).";

static const EquationEntry eqs_multielectron[] = {
    { "Helium Hamiltonian",
      eq_helium_hamiltonian,
      "H_hat    : total Hamiltonian (He atom)\n"
      "hbar     : reduced Planck's constant\n"
      "m_e      : electron mass\n"
      "nabla_i  : gradient for electron i (kinetic op)\n"
      "r_i      : electron i to nucleus distance\n"
      "r_12     : electron 1 to electron 2 distance\n"
      "(attractions truncated in display)" },
    { "Separable Wavefunction",
      eq_separable_wavefunction,
      "Psi     : total many-electron wavefunction\n"
      "psi_n   : 1-electron wavefunction for electron n\n"
      "r_n     : spatial coordinate of electron n" },
    { "Effective 1-Electron Hamiltonian",
      eq_effective_hamiltonian,
      "H_n       : 1-electron Hamiltonian (SCF)\n"
      "hbar, m   : reduced Planck's constant, electron mass\n"
      "nabla_n^2 : Laplacian for electron n\n"
      "V_eff,n   : averaged effective potential seen by electron n" },
    { "Slater Determinant",
      eq_slater_determinant,
      "Psi(1..N) : antisymmetric N-electron wavefunction\n"
      "N!        : factorial of the electron count\n"
      "1/sqrt(N!): normalisation prefactor\n"
      "chi_i(j)  : spin-orbital i evaluated for electron j\n"
      "Columns = spin-orbitals (1s alpha, 1s beta, 2s alpha, ...)\n"
      "Rows    = electrons (1, 2, ..., N).\n"
      "Swapping any two rows flips the sign -> Pauli exclusion." },
    { "Molecular Hamiltonian",
      eq_molecular_hamiltonian,
      "Sum nabla_i^2     : kinetic energy over all electrons\n"
      "Z_A e^2 / r_iA    : electron-nucleus attraction\n"
      "e^2 / r_ij        : electron-electron repulsion (i<j)\n"
      "Z_A Z_B e^2 / R_AB: nucleus-nucleus repulsion\n"
      "hbar, m_e, epsilon0: usual constants\n"
      "r_iA : electron i to nucleus A ; R_AB : nuclei A-B" },
    { "Overlap Integral S_12",
      eq_overlap_integral,
      "phi_1s(i) : 1s atomic orbital evaluated for electron i\n"
      "S_12      : spatial overlap between two atomic orbitals\n"
      "S_12 must be nonzero to form a bonding MO; S_12 -> 1 as\n"
      "the orbitals become identical." },
    { "Bonding Coefficient c_g",
      eq_cg_coefficient,
      "c_g  : normalisation coefficient for the symmetric (gerade,\n"
      "       bonding) MO psi_g = c_g (phi_1s(1) + phi_1s(2))\n"
      "S_12 : overlap integral between the two atomic orbitals\n"
      "Derived from <psi_g|psi_g> = 1." },
    { "Antibonding Coefficient c_u",
      eq_cu_coefficient,
      "c_u  : normalisation coefficient for the antisymmetric\n"
      "       (ungerade, antibonding) MO psi_u = c_u (phi_1s(1)\n"
      "       - phi_1s(2))\n"
      "S_12 : overlap integral between the two atomic orbitals\n"
      "Derived from <psi_u|psi_u> = 1." },
    { "sp Hybrid Orbital",
      eq_sp_hybrid_def,
      "psi_sp,1 : one of the two sp hybrid orbitals\n"
      "c1, c2   : coefficients for the 2s and 2p_x contributions\n"
      "phi_2s   : 2s atomic orbital\n"
      "phi_2px  : 2p_x atomic orbital\n"
      "The partner hybrid psi_sp,2 uses coefficients c5, c6." },
    { "sp Normalization",
      eq_sp_normalization,
      "Each hybrid orbital must be normalised:\n"
      "<psi_sp,1 | psi_sp,1> = c1^2 + c2^2 = 1\n"
      "Likewise for psi_sp,2: c5^2 + c6^2 = 1." },
    { "sp Orthogonality",
      eq_sp_orthogonality,
      "The two hybrid orbitals must be orthogonal:\n"
      "<psi_sp,1 | psi_sp,2> = c1 c5 + c2 c6 = 0.\n"
      "This ensures the hybrids are independent." },
    { "Equal Contribution Rule",
      eq_sp_equal_contribution,
      "The 2s orbital contributes equally across both hybrids:\n"
      "c1^2 + c5^2 = 1.  The 2p_x orbital likewise:\n"
      "c2^2 + c6^2 = 1.  Combined with normalisation and\n"
      "orthogonality this uniquely fixes the coefficients." },
};

static const KeywordEntry kws_multielectron[] = {
    { "Iteration",
      "Repeating a mathematical procedure on its previous result to "
      "obtain successively better approximations." },
    { "Recursion",
      "A function or operation that acts on its own output." },
    { "Trial Wavefunction (Phi)",
      "Approximate wavefunction used in the Variation Theorem to "
      "estimate the true wavefunction Psi." },
    { "Variation Theorem",
      "<Phi|H|Phi> / <Phi|Phi> is always >= the true ground-state "
      "energy, with equality only when Phi = Psi." },
    { "Self-Consistent Field",
      "Hartree-Fock approach: each electron interacts with an averaged "
      "effective field from the others, updated iteratively until the "
      "result stops changing." },
    { "Slater Determinant",
      "Antisymmetrised product of spin orbitals; enforces the Pauli "
      "exclusion principle automatically.  Prefactor 1/sqrt(N!), "
      "columns are spin-orbitals, rows are electron labels." },
    { "Pauli Exclusion Principle",
      "No two fermions may occupy the same quantum state.  The Slater "
      "determinant vanishes whenever two columns are identical." },
    { "Spin-Orbital",
      "Product of a spatial orbital and a spin function (alpha or "
      "beta), e.g. 1s-alpha, 1s-beta, 2s-alpha." },
    { "Electron Spin",
      "Intrinsic half-integer angular momentum that must be included "
      "in the full wavefunction for many-electron atoms." },
    { "Born-Oppenheimer Approximation",
      "Separates nuclear and electronic wavefunctions because nuclei "
      "are much heavier and move far slower than electrons." },
    { "LCAO-MO",
      "Linear Combination of Atomic Orbitals: a molecular orbital is "
      "written as a weighted sum of atomic orbital wavefunctions, with "
      "coefficients optimised to minimise the energy." },
    { "Overlap Integral (S_12)",
      "Spatial overlap between two atomic orbitals, S_12 = "
      "<phi_1(1)|phi_1(2)>.  Nonzero overlap is required to form a "
      "bonding MO; S_12 = 1 means the two orbitals are identical." },
    { "Gerade / Ungerade (g / u)",
      "Symmetric (gerade, bonding) MOs add the atomic orbitals; "
      "antisymmetric (ungerade, antibonding) MOs subtract them.  "
      "Normalisation gives c_g = 1/sqrt(2(1 + S_12)) and "
      "c_u = 1/sqrt(2(1 - S_12))." },
    { "Secular Determinant",
      "Algebraic tool that chooses the c_i coefficients of an LCAO-MO "
      "to minimise the expectation energy <E>." },
    { "Bond Order",
      "Half the difference between bonding and antibonding electron "
      "counts.  N2: 3, He2: 0." },
    { "Hybridization (sp, sp2, sp3)",
      "Mixing of atomic orbitals on the same atom to form directed "
      "hybrid orbitals.  Coefficients are fixed by normalisation "
      "(|c|^2 = 1), orthogonality (<psi_i|psi_j> = 0), and equal-"
      "contribution rules." },
    { "Normalization Constraint",
      "<psi|psi> = 1.  For an sp hybrid: c1^2 + c2^2 = 1." },
    { "Orthogonality Constraint",
      "<psi_i|psi_j> = 0 for i != j.  For two sp hybrids: "
      "c1 c5 + c2 c6 = 0." },
    { "Equal Contribution Rule",
      "Each atomic orbital contributes the same total amplitude to "
      "the full hybrid set: c1^2 + c5^2 = 1 and c2^2 + c6^2 = 1." },
};

/* =========================================================================
 * §4  TopicContent AGGREGATES + LOOKUP HELPERS
 * ========================================================================= */

/* Short descriptions for topics that did not receive full paragraph text */
static const char *desc_pib =
    "A particle confined to a 1D box of length L has quantized energy "
    "levels: the wavefunction must vanish at the walls, forcing a "
    "discrete set of standing waves indexed by n = 1, 2, 3, ...\n\n"
    "Zero-point energy (E_1 > 0) means the particle cannot be at rest. "
    "The n-th wavefunction has (n-1) interior nodes. The same idea "
    "extends to 2D and 3D boxes, and underlies the band theory of "
    "solids.\n\n"
    "The 1D-box formula is also used to estimate HOMO and LUMO energies "
    "of conjugated pi-systems. Combining E_n with the Boltzmann "
    "distribution gives the thermal population of each level at "
    "temperature T: the ratio p_LUMO / p_HOMO = exp(-Delta E / kT).";

static const char *desc_commutators =
    "Two quantum observables can be simultaneously known only if their "
    "operators commute. The commutator [A_hat, B_hat] measures how far "
    "they are from commuting.\n\n"
    "Non-commuting operators yield a lower bound on the product of "
    "their uncertainties. For position and momentum this is the "
    "Heisenberg Uncertainty Principle.\n\n"
    "Electron spin is an intrinsic angular momentum with no classical "
    "analogue. Spin-1/2 particles have two basis states (alpha and "
    "beta), demonstrated by the Stern-Gerlach experiment.";

static const char *desc_oscillator =
    "The quantum harmonic oscillator has equally spaced energy levels "
    "separated by hbar*omega; its wavefunctions are Hermite polynomials "
    "modulated by a Gaussian envelope.\n\n"
    "The rigid rotor models a diatomic molecule as two fixed-distance "
    "masses rotating in space. Its levels depend on the moment of "
    "inertia I and the quantum number J.\n\n"
    "Together these two models -- oscillator for vibration, rotor for "
    "rotation -- describe most of the vibrational and rotational "
    "behaviour of diatomic molecules.";

/* Topic titles (also used in the menu) */
static const char *topic_titles[NUM_TOPICS] = {
    "Particle in a Box",
    "Commutators & Spin",
    "Harmonic Osc. & Rotor",
    "Diatomic Spectroscopy",
    "Hydrogen Atom",
    "Many-Electron Atoms",
};

/* Master TopicContent table */
static const TopicContent topic_table[NUM_TOPICS] = {
    {   /* 0 */
        .title             = "Particle in a Box",
        .description_block = NULL,                          /* set below */
        .equations         = eqs_pib,
        .num_equations     = (int)(sizeof(eqs_pib) / sizeof(eqs_pib[0])),
        .keywords          = kws_pib,
        .num_keywords      = (int)(sizeof(kws_pib) / sizeof(kws_pib[0])),
    },
    {   /* 1 */
        .title             = "Commutators & Spin",
        .description_block = NULL,
        .equations         = eqs_commutators,
        .num_equations     = (int)(sizeof(eqs_commutators)
                                   / sizeof(eqs_commutators[0])),
        .keywords          = kws_commutators,
        .num_keywords      = (int)(sizeof(kws_commutators)
                                   / sizeof(kws_commutators[0])),
    },
    {   /* 2 */
        .title             = "Harmonic Osc. & Rotor",
        .description_block = NULL,
        .equations         = eqs_oscillator,
        .num_equations     = (int)(sizeof(eqs_oscillator)
                                   / sizeof(eqs_oscillator[0])),
        .keywords          = kws_oscillator,
        .num_keywords      = (int)(sizeof(kws_oscillator)
                                   / sizeof(kws_oscillator[0])),
    },
    {   /* 3 */
        .title             = "Diatomic Spectroscopy",
        .description_block = NULL,
        .equations         = eqs_spectroscopy,
        .num_equations     = (int)(sizeof(eqs_spectroscopy)
                                   / sizeof(eqs_spectroscopy[0])),
        .keywords          = kws_spectroscopy,
        .num_keywords      = (int)(sizeof(kws_spectroscopy)
                                   / sizeof(kws_spectroscopy[0])),
    },
    {   /* 4 */
        .title             = "Hydrogen Atom",
        .description_block = NULL,
        .equations         = eqs_hydrogen,
        .num_equations     = (int)(sizeof(eqs_hydrogen)
                                   / sizeof(eqs_hydrogen[0])),
        .keywords          = kws_hydrogen,
        .num_keywords      = (int)(sizeof(kws_hydrogen)
                                   / sizeof(kws_hydrogen[0])),
    },
    {   /* 5 */
        .title             = "Many-Electron Atoms",
        .description_block = NULL,
        .equations         = eqs_multielectron,
        .num_equations     = (int)(sizeof(eqs_multielectron)
                                   / sizeof(eqs_multielectron[0])),
        .keywords          = kws_multielectron,
        .num_keywords      = (int)(sizeof(kws_multielectron)
                                   / sizeof(kws_multielectron[0])),
    },
};

/* Because C cannot use aggregate initialisers to link to function-local
 * static strings cleanly, we patch description_block pointers at the
 * point of lookup below. */
const TopicContent *topic_content(TopicID id)
{
    static TopicContent out;
    if (id < 0 || id >= NUM_TOPICS) return NULL;
    out = topic_table[id];
    switch (id) {
        case TOPIC_PIB:           out.description_block = desc_pib; break;
        case TOPIC_COMMUTATORS:   out.description_block = desc_commutators; break;
        case TOPIC_OSCILLATOR:    out.description_block = desc_oscillator; break;
        case TOPIC_SPECTROSCOPY:  out.description_block = desc_spectroscopy; break;
        case TOPIC_HYDROGEN:      out.description_block = desc_hydrogen; break;
        case TOPIC_MULTIELECTRON: out.description_block = desc_multielectron; break;
        default:                  out.description_block = ""; break;
    }
    return &out;
}

const char *topic_title(TopicID id)
{
    if (id >= 0 && id < NUM_TOPICS) return topic_titles[id];
    return "Unknown";
}

/* =========================================================================
 * §5  SUBMENU IMPLEMENTATION
 * =========================================================================
 * The three entries are fixed: Descriptions / Equations / Keywords.
 * Selection returns a SubtopicID via sm->sel (0, 1, 2).
 * ========================================================================= */

static const char *subtopic_labels[NUM_SUBTOPICS] = {
    "1. Descriptions",
    "2. Equations",
    "3. Keywords",
};

void submenu_init(SubMenuScreen *sm, TopicID topic)
{
    sm->topic = topic;
    sm->sel   = 0;
}

void submenu_draw(const SubMenuScreen *sm)
{
    /* Header bar */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, topic_title(sm->topic));
    dtext(SCREEN_W - 100, 7, COL_HEADER_FG, "[EXIT] Back");

    /* Three menu rows */
    for (int i = 0; i < NUM_SUBTOPICS; i++) {
        int y = HEADER_H + i * MENU_ITEM_H;
        int is_sel = (i == sm->sel);
        int bg = is_sel ? COL_SEL_BG : COL_ITEM_BG;
        int fg = is_sel ? COL_SEL_FG : COL_ITEM_FG;

        drect(0, y, SCREEN_W - 1, y + MENU_ITEM_H - 1, bg);
        if (!is_sel) {
            drect(MENU_PAD_LEFT, y + MENU_ITEM_H - 1,
                  SCREEN_W - 8, y + MENU_ITEM_H - 1, COL_DIVIDER);
        }
        int text_y = y + (MENU_ITEM_H - 14) / 2;
        dtext(MENU_PAD_LEFT, text_y, fg, subtopic_labels[i]);
        if (is_sel) {
            dtext(SCREEN_W - 28, text_y, fg, "\xE6\x91");
        }
    }

    /* Footer hint */
    int fy = SCREEN_H - FOOTER_H;
    drect(0, fy, SCREEN_W - 1, SCREEN_H - 1, COL_HEADER_BG);
    dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
          "EXE: open    EXIT: back    MENU: quit");
}

int submenu_handle_key(SubMenuScreen *sm, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    switch (ev.key) {
    case KEY_UP:
        if (sm->sel > 0) sm->sel--;
        return 0;
    case KEY_DOWN:
        if (sm->sel < NUM_SUBTOPICS - 1) sm->sel++;
        return 0;
    case KEY_EXE:
        return 1;
    case KEY_EXIT:
        return 2;
    case KEY_MENU:
        return 3;
    default:
        return 0;
    }
}

/* =========================================================================
 * §6  TOPIC CONTENT SCREEN IMPLEMENTATION
 * =========================================================================
 *
 * Strategy: the draw function does a single pass that both measures and
 * (optionally) renders content.  Everything above the viewport is
 * skipped; everything below is short-circuited.  content_h is accumulated
 * so that scroll clamping is always correct.
 *
 * The math-node pool is reset once at the start of each draw so AST
 * trees for previous equations are discarded cleanly.
 * ========================================================================= */

void topic_init(TopicScreen *ts, TopicID topic, SubtopicID subtopic)
{
    ts->topic    = topic;
    ts->subtopic = subtopic;
    ts->scroll_y = 0;
    ts->content_h = 0;
}

/* Section heading block */
static int draw_heading(const char *text, int x, int y, int color)
{
    dtext(x, y, color, text);
    /* Thin underline */
    drect(x, y + HEADING_H_LOCAL - 3,
          SCREEN_W - CONTENT_PAD, y + HEADING_H_LOCAL - 3, color);
    return HEADING_H_LOCAL + 2;
}

/* ---------- Descriptions view ---------- */
static int draw_descriptions(const TopicContent *tc, int x, int y, int max_w)
{
    int cy = y;
    cy += draw_heading("Overview", x, cy, COL_ACCENT);
    cy += 4;
    cy = draw_wrapped(tc->description_block, x, cy, max_w,
                      COL_ITEM_FG, 1);
    return cy - y;
}

static int measure_descriptions(const TopicContent *tc, int max_w)
{
    int cy = 0;
    cy += HEADING_H_LOCAL + 2;
    cy += 4;
    cy = draw_wrapped(tc->description_block, 0, cy, max_w,
                      0, 0);
    return cy;
}

/* ---------- Equations view ---------- */
static int draw_equations(const TopicContent *tc, int x, int y, int max_w,
                          int draw)
{
    int cy = y;
    if (draw) cy += draw_heading("Equations", x, cy, COL_ACCENT);
    else cy += HEADING_H_LOCAL + 2;
    cy += 4;

    for (int i = 0; i < tc->num_equations; i++) {
        const EquationEntry *eq = &tc->equations[i];

        /* Label row */
        if (draw) dtext(x, cy, COL_ACCENT, eq->label);
        cy += BODY_LINE_H + 2;

        /* Render the math expression */
        if (eq->builder) {
            MathNode *tree = eq->builder();
            if (tree) {
                render_layout(tree);
                if (draw) {
                    int ex = x + 8;
                    if (ex + tree->layout.w > x + max_w) ex = x;
                    render_draw(tree, ex, cy);
                }
                cy += tree->layout.h + 4;
            }
        }

        /* Variable descriptions (indented, muted colour) */
        if (eq->vars && *eq->vars) {
            cy += 2;
            cy = draw_wrapped(eq->vars, x + 10, cy, max_w - 10,
                              COL_MUTED, draw);
        }
        cy += PARA_GAP + 4;

        /* Divider between entries (except last) */
        if (i < tc->num_equations - 1) {
            if (draw) {
                drect(x, cy, x + max_w - 1, cy, COL_DIVIDER);
            }
            cy += 8;
        }
    }
    return cy - y;
}

/* ---------- Keywords view ---------- */
static int draw_keywords(const TopicContent *tc, int x, int y, int max_w,
                         int draw)
{
    int cy = y;
    if (draw) cy += draw_heading("Key Words", x, cy, COL_ACCENT);
    else cy += HEADING_H_LOCAL + 2;
    cy += 4;

    for (int i = 0; i < tc->num_keywords; i++) {
        const KeywordEntry *kw = &tc->keywords[i];

        /* Term in accent colour */
        if (draw) dtext(x, cy, COL_ACCENT, kw->name);
        cy += BODY_LINE_H + 1;

        /* Definition, indented */
        cy = draw_wrapped(kw->definition, x + 10, cy, max_w - 10,
                          COL_ITEM_FG, draw);
        cy += PARA_GAP;
    }
    return cy - y;
}

/* ---------- Unified draw dispatch ---------- */
void topic_draw(TopicScreen *ts)
{
    const TopicContent *tc = topic_content(ts->topic);
    if (!tc) return;

    /* Header */
    drect(0, 0, SCREEN_W - 1, HEADER_H - 1, COL_HEADER_BG);
    const char *st_name =
        (ts->subtopic == SUBTOPIC_DESCRIPTIONS) ? "Descriptions" :
        (ts->subtopic == SUBTOPIC_EQUATIONS)    ? "Equations"    :
                                                   "Keywords";
    char header_buf[80];
    int n = 0;
    /* Manual copy rather than snprintf to avoid libc surprises on sh-elf */
    const char *tt = topic_title(ts->topic);
    while (*tt && n < 40) header_buf[n++] = *tt++;
    header_buf[n++] = ' ';
    header_buf[n++] = '-';
    header_buf[n++] = ' ';
    while (*st_name && n < 78) header_buf[n++] = *st_name++;
    header_buf[n] = '\0';
    dtext(MENU_PAD_LEFT, 7, COL_HEADER_FG, header_buf);
    dtext(SCREEN_W - 100, 7, COL_HEADER_FG, "[EXIT] Back");

    int x     = CONTENT_PAD;
    int max_w = SCREEN_W - CONTENT_PAD * 2;
    int y     = HEADER_H + CONTENT_PAD - ts->scroll_y;

    /* Fresh math-node pool for this draw */
    render_pool_reset();

    int content_h = 0;
    switch (ts->subtopic) {
    case SUBTOPIC_DESCRIPTIONS:
        content_h = draw_descriptions(tc, x, y, max_w);
        break;
    case SUBTOPIC_EQUATIONS:
        content_h = draw_equations(tc, x, y, max_w, 1);
        break;
    case SUBTOPIC_KEYWORDS:
        content_h = draw_keywords(tc, x, y, max_w, 1);
        break;
    default:
        break;
    }

    /* For accurate scroll clamping, compute the total height once,
     * independently of the viewport-based partial draw. */
    render_pool_reset();   /* equations may have allocated for measure */
    int total = 0;
    switch (ts->subtopic) {
    case SUBTOPIC_DESCRIPTIONS:
        total = measure_descriptions(tc, max_w);
        break;
    case SUBTOPIC_EQUATIONS:
        total = draw_equations(tc, 0, 0, max_w, 0);
        break;
    case SUBTOPIC_KEYWORDS:
        total = draw_keywords(tc, 0, 0, max_w, 0);
        break;
    default:
        break;
    }
    ts->content_h = (total > content_h) ? total : content_h;

    /* Footer hint (draw over any overflowing text) */
    int fy = SCREEN_H - FOOTER_H;
    drect(0, fy, SCREEN_W - 1, SCREEN_H - 1, COL_HEADER_BG);
    if (ts->content_h > SCREEN_H - HEADER_H - FOOTER_H) {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "UP/DOWN: scroll   F1/F2: page   EXIT: back");
    } else {
        dtext(CONTENT_PAD, fy + 3, COL_HEADER_FG,
              "EXIT: back   MENU: quit");
    }
}

int topic_handle_key(TopicScreen *ts, key_event_t ev)
{
    if (ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return 0;

    int page  = SCREEN_H - HEADER_H - FOOTER_H;
    int limit = ts->content_h - page;
    if (limit < 0) limit = 0;

    switch (ev.key) {
    case KEY_UP:
        ts->scroll_y -= 18;
        if (ts->scroll_y < 0) ts->scroll_y = 0;
        return 0;
    case KEY_DOWN:
        ts->scroll_y += 18;
        if (ts->scroll_y > limit) ts->scroll_y = limit;
        return 0;
    case KEY_F1:
        ts->scroll_y -= page;
        if (ts->scroll_y < 0) ts->scroll_y = 0;
        return 0;
    case KEY_F2:
        ts->scroll_y += page;
        if (ts->scroll_y > limit) ts->scroll_y = limit;
        return 0;
    case KEY_EXIT:
        return 1;
    case KEY_MENU:
        return 2;
    default:
        return 0;
    }
}
