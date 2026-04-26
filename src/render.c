/* ==========================================================================
 * render.c — 2D Math Rendering Engine: Complete Implementation
 * ==========================================================================
 *
 * This file implements the Eigenmath-style two-pass recursive layout
 * engine for rendering mathematical expressions on the Casio fx-CG50.
 *
 * FILE ORGANIZATION
 * =================
 *   §1  Pool Allocator            — static bump allocator, O(1) reset
 *   §2  Symbol Table              — 42-entry name→OS-byte lookup
 *   §3  Font Metrics              — glyph measurement per font tier
 *   §4  Convenience Constructors  — 20 ergonomic AST node builders
 *   §5  Layout Constants          — spacing / padding tunables
 *   §6  Pass 1: render_layout()   — bottom-up bounding box computation
 *   §7  Drawing Helpers           — double-stroked bracket/sign primitives
 *   §8  Pass 2: render_draw()     — top-down VRAM blitting
 *   §9  Debug Utilities           — type name lookup
 *
 * COMPILATION TARGET
 * ==================
 * sh3eb-elf-none GCC via fxSDK + gint.  Links against libgint.
 * Uses gint's display API:
 *   dtext(x, y, color, str)      — render text
 *   dline(x1, y1, x2, y2, col)  — draw line
 *   drect(x1, y1, x2, y2, col)  — draw filled rectangle
 *   dpixel(x, y, col)           — set single pixel
 *
 * All drawing writes to an off-screen VRAM buffer.  The caller must
 * call dupdate() after all rendering is done to commit to the LCD.
 * ========================================================================== */

#include "render.h"
#include <gint/display.h>
#include <string.h>


/* #########################################################################
 * §1: POOL ALLOCATOR
 * #########################################################################
 *
 * A static array of 256 MathNode structs with a bump pointer.
 *
 * WHY NOT MALLOC:
 *   1. The CG50's SH4 CPU has no MMU.  gint's malloc uses a simple heap
 *      that can fragment.  For trees we build and tear down per screen,
 *      a bump allocator is strictly better.
 *   2. Deterministic memory: 256 × ~88 bytes = ~22 KB at compile time.
 *   3. O(1) reset: set the bump pointer to 0 and memset once.
 *   4. Cache-friendly: contiguous nodes help the SH4's small data cache.
 *
 * POOL SIZE JUSTIFICATION:
 *   The largest single expression (e.g. ⟨ψ|Ĥ|ψ⟩ = ∫ψ*Ĥψ dx) uses ~25
 *   nodes.  A topic screen with ~8 equations uses ~200 nodes worst case.
 *   256 provides comfortable headroom.
 * ######################################################################### */

static MathNode node_pool[MATH_NODE_POOL_SIZE];
static int pool_next = 0;

void render_pool_reset(void)
{
    pool_next = 0;
    /* Zero the entire pool so stale child pointers from a previous
     * screen can never be accidentally dereferenced.  ~22 KB memset
     * is negligible on the 60 MHz SH4. */
    memset(node_pool, 0, sizeof(node_pool));
}

MathNode *render_node_alloc(void)
{
    if (pool_next >= MATH_NODE_POOL_SIZE) return NULL;

    MathNode *n = &node_pool[pool_next++];
    memset(n, 0, sizeof(MathNode));
    n->font_tier = FONT_NORMAL;   /* sane default */
    return n;
}

int render_pool_used(void)
{
    return pool_next;
}


/* #########################################################################
 * §2: SYMBOL TABLE  (42 entries)
 * #########################################################################
 *
 * Maps human-readable names ("psi", "Delta") to the Casio OS's
 * proprietary 2-byte multi-byte character codes.
 *
 * HOW CG50 OS FONT ENCODING WORKS
 * --------------------------------
 * Special characters are 2-byte sequences: a "lead byte" (0xE5–0xE7)
 * followed by a glyph index.  gint's dtext() accepts these inline in
 * a C string.  The codes below are from:
 *   - WikiPrizm  (prizm.cemetech.net/Prizm_Programming_Portal)
 *   - Cemetech forum viewtopic.php?t=18603 (OS font glyph thread)
 *   - Empirical testing on CG50 OS 3.30+
 *
 * Each entry stores a pre-measured pixel width at the 18px font.
 * These were measured on-device; if a glyph appears wrong, adjust
 * the byte pair here and rebuild.
 *
 * USAGE:
 *   math_symbol("psi")  →  MathNode with d.leaf.text = "\xE5\xB8"
 *   sym("Delta")        →  "\xE5\x84"  (raw byte string)
 *   sym_width("pi")     →  10           (pixel width)
 * ######################################################################### */

static SymbolEntry sym_entries[MAX_SYMBOLS];
static int sym_count = 0;

/* Internal: register one symbol in the table */
static void sym_add(const char *name, const char *bytes, int w)
{
    if (sym_count >= MAX_SYMBOLS) return;
    sym_entries[sym_count].name     = name;
    sym_entries[sym_count].bytes    = bytes;
    sym_entries[sym_count].width_px = w;
    sym_count++;
}

void sym_table_init(void)
{
    sym_count = 0;

    /* ============================================================
     * GREEK LOWERCASE  (21 entries)
     * ============================================================
     * Common in quantum mechanics:
     *   ψ wavefunction, φ angle/orbital, α/β spin states,
     *   θ angle, λ wavelength, ω angular frequency,
     *   σ cross-section, μ reduced mass, ν frequency,
     *   ε energy, γ decay rate, δ variation, π pi
     * ============================================================ */
    sym_add("alpha",   "\xE5\xA0", 10);  /*  α  */
    sym_add("beta",    "\xE5\xA1", 10);  /*  β  */
    sym_add("gamma",   "\xE5\xA2", 10);  /*  γ  */
    sym_add("delta",   "\xE5\xA3", 10);  /*  δ  */
    sym_add("epsilon", "\xE5\xA4", 10);  /*  ε  */
    sym_add("zeta",    "\xE5\xA5", 10);  /*  ζ  */
    sym_add("eta",     "\xE5\xA6", 10);  /*  η  */
    sym_add("theta",   "\xE5\xA8", 10);  /*  θ  */
    sym_add("iota",    "\xE5\xA9", 10);  /*  ι  */
    sym_add("kappa",   "\xE5\xAA", 10);  /*  κ  */
    sym_add("lambda",  "\xE5\xAB", 10);  /*  λ  */
    sym_add("mu",      "\xE5\xAC", 10);  /*  μ  */
    sym_add("nu",      "\xE5\xAD", 10);  /*  ν  */
    sym_add("xi",      "\xE5\xAE", 10);  /*  ξ  */
    sym_add("pi",      "\xE5\xB0", 10);  /*  π  */
    sym_add("rho",     "\xE5\xB1", 10);  /*  ρ  */
    sym_add("sigma",   "\xE5\xB3", 10);  /*  σ  */
    sym_add("tau",     "\xE5\xB4", 10);  /*  τ  */
    sym_add("phi",     "\xE5\xB6", 10);  /*  φ  */
    sym_add("chi",     "\xE5\xB7", 10);  /*  χ  */
    sym_add("psi",     "\xE5\xB8", 10);  /*  ψ  */
    sym_add("omega",   "\xE5\xB9", 10);  /*  ω  */

    /* ============================================================
     * GREEK UPPERCASE  (9 entries)
     * ============================================================ */
    sym_add("Gamma",   "\xE5\x83", 12);  /*  Γ  */
    sym_add("Delta",   "\xE5\x84", 12);  /*  Δ  */
    sym_add("Theta",   "\xE5\x88", 12);  /*  Θ  */
    sym_add("Lambda",  "\xE5\x8B", 12);  /*  Λ  */
    sym_add("Pi",      "\xE5\x90", 12);  /*  Π  */
    sym_add("Sigma",   "\xE5\x93", 14);  /*  Σ  */
    sym_add("Phi",     "\xE5\x96", 12);  /*  Φ  */
    sym_add("Psi",     "\xE5\x98", 12);  /*  Ψ  */
    sym_add("Omega",   "\xE5\x99", 12);  /*  Ω  */

    /* ============================================================
     * MATH OPERATORS  (9 entries)
     * ============================================================ */
    sym_add("pm",       "\xE5\xC0", 12);  /*  ±  */
    sym_add("times",    "\xE5\xC1", 10);  /*  ×  */
    sym_add("div",      "\xE5\xC2", 10);  /*  ÷  */
    sym_add("leq",      "\xE5\xC4", 12);  /*  ≤  */
    sym_add("geq",      "\xE5\xC5", 12);  /*  ≥  */
    sym_add("neq",      "\xE5\xC6", 12);  /*  ≠  */
    sym_add("inf",      "\xE5\xD0", 14);  /*  ∞  */
    sym_add("rarr",     "\xE5\xD1", 14);  /*  →  */
    sym_add("sqrt_sym", "\xE5\xCC", 10);  /*  √  */

    /* ============================================================
     * SPECIAL PHYSICS  (3 entries)
     * ============================================================
     * ℏ (h-bar) is not in the standard OS font.  We store "h" and
     * the caller wraps it: math_bar(math_text("h")) renders ℏ.
     * ∇ and ∂ similarly use ASCII fallbacks.
     * ============================================================ */
    sym_add("hbar",    "h",  9);   /*  ℏ  — wrap in math_bar()      */
    sym_add("nabla",   "V", 10);   /*  ∇  — fallback                */
    sym_add("partial", "d",  9);   /*  ∂  — fallback                */

    /* Total: 22 + 9 + 9 + 3 = 43 entries                           */
}

const char *sym(const char *name)
{
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_entries[i].name, name) == 0)
            return sym_entries[i].bytes;
    }
    return name;   /* graceful fallback: renders as ASCII */
}

int sym_width(const char *name)
{
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_entries[i].name, name) == 0)
            return sym_entries[i].width_px;
    }
    return (int)strlen(name) * 9;   /* fallback: ~9px per ASCII char */
}

/* -----------------------------------------------------------------------
 * greek_substitute_word - prose-side glyph substitution
 *
 * Looks up exactly the (word, len) span in the symbol table.  We only
 * substitute a curated subset of the symbol table (just the Greek
 * letters and a few math operators) so that ASCII identifiers like
 * "Phi" inside variable names ("Phi_2s") never get spuriously matched
 * because callers always pass whole-word spans split on whitespace.
 *
 * Returns the OS multi-byte string for the glyph on match, NULL on
 * miss.  *out_len is set to the byte length of the returned string
 * (always 2 for our OS sequences).
 * ----------------------------------------------------------------------- */
const char *greek_substitute_word(const char *word, int len, int *out_len)
{
    /* Curated list of names that are safe to substitute in prose.
     * Excludes "P", "p", "i", "pm", "leq" etc. that would mangle text. */
    static const char *greek_names[] = {
        "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta",
        "theta", "iota", "kappa", "lambda", "mu", "nu", "xi", "rho",
        "sigma", "tau", "phi", "chi", "psi", "omega",
        "Gamma", "Delta", "Theta", "Lambda", "Pi", "Sigma", "Phi",
        "Psi", "Omega",
        NULL
    };

    if (!word || len <= 0) return NULL;

    for (int i = 0; greek_names[i]; i++) {
        const char *cand = greek_names[i];
        int clen = (int)strlen(cand);
        if (clen != len) continue;
        if (memcmp(word, cand, len) != 0) continue;

        /* Match - return the OS bytes for this name */
        const char *bytes = sym(cand);
        /* Sanity: the symbol table should always return a real 2-byte
         * sequence for the names listed above.  If it's not registered
         * for some reason (e.g. "pi" is used here -- intentionally
         * omitted from this list because "pi" appears in English text
         * like "pi-system"), fall through. */
        if (bytes == cand) return NULL;       /* fallback path */
        if (out_len) *out_len = (int)strlen(bytes);
        return bytes;
    }
    return NULL;
}


/* #########################################################################
 * §3: FONT METRICS
 * #########################################################################
 *
 * Glyph measurement functions that map FontTier → pixel dimensions.
 *
 * These constants were measured empirically on CG50 OS 3.30+.
 * For exact per-glyph widths, one would call gint's dsize(), but that
 * requires the correct font to be active.  Pre-measured constants let
 * render_layout() be independent of the active font state.
 *
 *                    Width (avg)   Height
 *   FONT_LARGE:       13 px        20 px
 *   FONT_NORMAL:       9 px        14 px
 *   FONT_SMALL:        7 px        11 px
 * ######################################################################### */

static int tier_glyph_h(FontTier t)
{
    switch (t) {
        case FONT_LARGE:  return 20;
        case FONT_NORMAL: return 14;
        case FONT_SMALL:  return 11;
    }
    return 14;
}

static int tier_glyph_w(FontTier t)
{
    switch (t) {
        case FONT_LARGE:  return 13;
        case FONT_NORMAL: return  9;
        case FONT_SMALL:  return  7;
    }
    return 9;
}

/* -----------------------------------------------------------------------
 * measure_text_width - pixel width of a string at a given tier
 *
 * Multi-byte detection:
 *   Bytes 0xE5-0xE7 are lead bytes.  Each 2-byte pair = one glyph.
 *   Everything else = one ASCII glyph per byte.
 *
 * v5 BUG FIX:
 *   Previously this routine treated every glyph (ASCII or OS multi-byte)
 *   as having the same width, so a leaf containing a single Greek symbol
 *   like Delta was allocated only ASCII_W = 9 px, while the actual
 *   rendered glyph is ~12 px wide.  The next character then drew on top
 *   of the right edge of Delta, making it look "missing" on screen.
 *
 *   We now apply a per-tier multi-byte width that reflects the wider
 *   metrics of the OS Greek/operator glyphs.  Numbers come from the
 *   pre-measured widths recorded in the symbol table (§2 above).
 * ----------------------------------------------------------------------- */
static int tier_multibyte_w(FontTier t)
{
    switch (t) {
        case FONT_LARGE:  return 16;   /* big tier ~ 1.3x normal */
        case FONT_NORMAL: return 12;   /* matches sym table width */
        case FONT_SMALL:  return 10;
    }
    return 12;
}

static int measure_text_width(const char *s, FontTier tier)
{
    int width = 0;
    const unsigned char *p = (const unsigned char *)s;
    int ascii_w     = tier_glyph_w(tier);
    int multibyte_w = tier_multibyte_w(tier);

    while (*p) {
        if (*p >= 0xE5 && *p <= 0xE7 && *(p + 1)) {
            width += multibyte_w;
            p += 2;       /* consume the 2-byte pair */
        } else {
            width += ascii_w;
            p++;
        }
    }
    return width;
}


/* #########################################################################
 * §4: CONVENIENCE CONSTRUCTORS
 * #########################################################################
 *
 * Each function:  alloc → set type → set font_tier → set payload → return.
 *
 * FONT TIER PROPAGATION:
 *   Constructors set FONT_NORMAL by default.  render_layout() demotes
 *   children of SUPERSCRIPT, SUBSCRIPT, and bigop limits to FONT_SMALL
 *   before recursing — the caller never manages tiers manually.
 *   math_text_small() and math_number_small() exist for the rare case
 *   where the caller wants to force a small tier explicitly.
 * ######################################################################### */

MathNode *math_text(const char *str)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_TEXT;
    strncpy(n->d.leaf.text, str, MAX_TEXT_LEN - 1);
    return n;
}

MathNode *math_number(const char *str)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_NUMBER;
    strncpy(n->d.leaf.text, str, MAX_TEXT_LEN - 1);
    return n;
}

MathNode *math_symbol(const char *name)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SYMBOL;
    /* Copy the OS byte sequence (not the human name) */
    strncpy(n->d.leaf.text, sym(name), MAX_TEXT_LEN - 1);
    return n;
}

MathNode *math_text_small(const char *str)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_TEXT;
    n->font_tier = FONT_SMALL;
    strncpy(n->d.leaf.text, str, MAX_TEXT_LEN - 1);
    return n;
}

MathNode *math_number_small(const char *str)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_NUMBER;
    n->font_tier = FONT_SMALL;
    strncpy(n->d.leaf.text, str, MAX_TEXT_LEN - 1);
    return n;
}

MathNode *math_fraction(MathNode *num, MathNode *den)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_FRACTION;
    n->d.frac.numerator   = num;
    n->d.frac.denominator = den;
    return n;
}

MathNode *math_superscript(MathNode *base, MathNode *exp)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SUPERSCRIPT;
    n->d.script.base   = base;
    n->d.script.script = exp;
    return n;
}

MathNode *math_subscript(MathNode *base, MathNode *sub)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SUBSCRIPT;
    n->d.script.base   = base;
    n->d.script.script = sub;
    return n;
}

MathNode *math_sqrt(MathNode *radicand)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SQRT;
    n->d.sqrt.radicand = radicand;
    return n;
}

MathNode *math_paren(MathNode *inner)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_PAREN;
    n->d.paren.inner = inner;
    return n;
}

MathNode *math_row(MathNode **children, int count)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_ROW;
    int c = (count > MAX_ROW_CHILDREN) ? MAX_ROW_CHILDREN : count;
    n->d.row.count = c;
    for (int i = 0; i < c; i++)
        n->d.row.children[i] = children[i];
    return n;
}

MathNode *math_integral(MathNode *lo, MathNode *hi, MathNode *body)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_INTEGRAL;
    n->d.bigop.lower = lo;
    n->d.bigop.upper = hi;
    n->d.bigop.body  = body;
    return n;
}

MathNode *math_summation(MathNode *lo, MathNode *hi, MathNode *body)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SUMMATION;
    n->d.bigop.lower = lo;
    n->d.bigop.upper = hi;
    n->d.bigop.body  = body;
    return n;
}

MathNode *math_bra(MathNode *content)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_BRA;
    n->d.bracket.content = content;
    return n;
}

MathNode *math_ket(MathNode *content)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_KET;
    n->d.bracket.content = content;
    return n;
}

MathNode *math_braket(MathNode *bra_label, MathNode *ket_label)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_BRAKET;
    n->d.braket.bra = bra_label;
    n->d.braket.ket = ket_label;
    return n;
}

MathNode *math_sandwich(MathNode *bra, MathNode *op, MathNode *ket)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_SANDWICH;
    n->d.sandwich.bra = bra;
    n->d.sandwich.op  = op;
    n->d.sandwich.ket = ket;
    return n;
}

MathNode *math_hat(MathNode *child)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_HAT;
    n->d.accent.child = child;
    return n;
}

MathNode *math_bar(MathNode *child)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_BAR;
    n->d.accent.child = child;
    return n;
}

MathNode *math_arrow(void)
{
    MathNode *n = render_node_alloc();  if (!n) return NULL;
    n->type = MATH_ARROW;
    return n;
}


/* #########################################################################
 * §5: LAYOUT CONSTANTS
 * #########################################################################
 * All values in pixels.  Tuned for the CG50's 396×224 screen.
 * ######################################################################### */

#define FRAC_BAR_PAD     3    /* vert gap between num/den and vinculum    */
#define FRAC_BAR_THICK   1    /* vinculum line thickness                  */
#define FRAC_H_PAD       4    /* horiz overhang of bar past content       */

#define SCRIPT_RISE      5    /* how far superscript rises above base top */
#define SCRIPT_DROP      3    /* how far subscript drops below base bot   */
#define SCRIPT_GAP       1    /* horiz gap between base and script        */

#define SQRT_HOOK_W     10    /* width of radical hook + diagonal         */
#define SQRT_TOP_PAD     3    /* clearance above radicand to overbar      */
#define SQRT_RIGHT_PAD   2    /* overbar overhang past radicand           */

#define PAREN_PAD        3    /* horiz width allocated to each bracket    */
#define PAREN_VERT_PAD   2    /* vert extension above/below content       */

#define ROW_GAP          2    /* horiz gap between ROW children           */

#define BRACKET_W        7    /* width of drawn ⟨ or ⟩                    */
#define BAR_W            2    /* width of vertical bar |                  */
#define BRACKET_PAD      2    /* padding between bracket and content      */

#define INTEGRAL_W      14    /* width of ∫ glyph                         */
#define SIGMA_W         16    /* width of drawn Σ glyph                   */
#define SIGMA_H         18    /* height of drawn Σ at body scale          */
#define BIGOP_PAD        3    /* gap between sign column and body         */
#define LIMIT_PAD        2    /* gap between sign and limit expressions   */

#define HAT_HEIGHT       5    /* height of circumflex accent              */
#define BAR_HEIGHT       3    /* height of overline accent                */

#define ARROW_W         18    /* total width of → arrow                   */
#define ARROW_HEAD       4    /* length of arrowhead barbs                */

static int imax(int a, int b) { return (a > b) ? a : b; }


/* #########################################################################
 * §6: PASS 1 — render_layout() — BOTTOM-UP BOUNDING BOX COMPUTATION
 * #########################################################################
 *
 * Walks the AST recursively.  For each node:
 *   1. Layout all children first (so their LayoutInfo is valid).
 *   2. Compute this node's (w, h, baseline) from children's measurements.
 *
 * FONT TIER AUTO-DEMOTION:
 *   Before recursing into a SUPERSCRIPT's exponent, SUBSCRIPT's subscript,
 *   or a bigop's upper/lower limits, this function sets the child's
 *   font_tier to FONT_SMALL.  This is the ONLY place font tiers are
 *   modified automatically — constructors always set FONT_NORMAL.
 *
 * BASELINE ALIGNMENT (used by MATH_ROW):
 *   ascent  = child->layout.baseline          (top to axis)
 *   descent = child->layout.h - child->baseline  (axis to bottom)
 *   row.baseline = max_ascent across all children
 *   row.h        = max_ascent + max_descent
 *   Each child is drawn at y = row_baseline - child.baseline during pass 2.
 * ######################################################################### */

void render_layout(MathNode *node)
{
    if (!node) return;

    switch (node->type) {

    /* ==================================================================
     * LEAF NODES
     * ================================================================== */
    case MATH_TEXT:
    case MATH_NUMBER:
    case MATH_SYMBOL:
    {
        FontTier t = node->font_tier;
        node->layout.w = measure_text_width(node->d.leaf.text, t);
        node->layout.h = tier_glyph_h(t);
        node->layout.baseline = node->layout.h / 2;
        break;
    }

    /* ==================================================================
     * MATH_FRACTION
     * ==================================================================
     *    ┌─── content_w + FRAC_H_PAD ───┐
     *    │      [numerator]              │
     *    │───────────────────────────────│  ← vinculum (= baseline)
     *    │      [denominator]            │
     *    └───────────────────────────────┘
     * ================================================================== */
    case MATH_FRACTION:
    {
        render_layout(node->d.frac.numerator);
        render_layout(node->d.frac.denominator);

        LayoutInfo *num = &node->d.frac.numerator->layout;
        LayoutInfo *den = &node->d.frac.denominator->layout;

        int cw = imax(num->w, den->w);

        node->layout.w = cw + FRAC_H_PAD;
        node->layout.h = num->h + FRAC_BAR_PAD
                        + FRAC_BAR_THICK
                        + FRAC_BAR_PAD
                        + den->h;
        node->layout.baseline = num->h + FRAC_BAR_PAD;
        break;
    }

    /* ==================================================================
     * MATH_SUPERSCRIPT
     * ==================================================================
     *          ┌script┐
     *    ┌base─┘      │     script at top, base offset down
     *    │             │
     *    └─────────────┘
     *
     *  The script child is AUTO-DEMOTED to FONT_SMALL here.
     * ================================================================== */
    case MATH_SUPERSCRIPT:
    {
        /* ---- AUTO-DEMOTION ---- */
        if (node->d.script.script)
            node->d.script.script->font_tier = FONT_SMALL;

        render_layout(node->d.script.base);
        render_layout(node->d.script.script);

        LayoutInfo *b = &node->d.script.base->layout;
        LayoutInfo *s = &node->d.script.script->layout;

        int base_y = s->h - SCRIPT_RISE;
        if (base_y < 0) base_y = 0;

        node->layout.w = b->w + SCRIPT_GAP + s->w;
        node->layout.h = base_y + b->h;
        node->layout.baseline = base_y + b->baseline;
        break;
    }

    /* ==================================================================
     * MATH_SUBSCRIPT
     * ==================================================================
     *    ┌base──┐
     *    │      └script┐     base at top, script drops below
     *    └──────┘      │
     *                  └──
     *
     *  The script child is AUTO-DEMOTED to FONT_SMALL here.
     * ================================================================== */
    case MATH_SUBSCRIPT:
    {
        /* ---- AUTO-DEMOTION ---- */
        if (node->d.script.script)
            node->d.script.script->font_tier = FONT_SMALL;

        render_layout(node->d.script.base);
        render_layout(node->d.script.script);

        LayoutInfo *b = &node->d.script.base->layout;
        LayoutInfo *s = &node->d.script.script->layout;

        int script_y = b->h - SCRIPT_DROP;
        int total_h  = imax(b->h, script_y + s->h);

        node->layout.w = b->w + SCRIPT_GAP + s->w;
        node->layout.h = total_h;
        node->layout.baseline = b->baseline;
        break;
    }

    /* ==================================================================
     * MATH_SQRT
     * ================================================================== */
    case MATH_SQRT:
    {
        render_layout(node->d.sqrt.radicand);
        LayoutInfo *r = &node->d.sqrt.radicand->layout;

        node->layout.w = SQRT_HOOK_W + r->w + SQRT_RIGHT_PAD;
        node->layout.h = SQRT_TOP_PAD + r->h;
        node->layout.baseline = SQRT_TOP_PAD + r->baseline;
        break;
    }

    /* ==================================================================
     * MATH_PAREN
     * ================================================================== */
    case MATH_PAREN:
    {
        render_layout(node->d.paren.inner);
        LayoutInfo *c = &node->d.paren.inner->layout;

        node->layout.w = PAREN_PAD + c->w + PAREN_PAD;
        node->layout.h = PAREN_VERT_PAD + c->h + PAREN_VERT_PAD;
        node->layout.baseline = PAREN_VERT_PAD + c->baseline;
        break;
    }

    /* ==================================================================
     * MATH_ROW — baseline-aligned horizontal sequence
     * ==================================================================
     *
     * THE CORE ALIGNMENT ALGORITHM:
     *
     * For each child, define:
     *   ascent  = child.baseline              (top to math axis)
     *   descent = child.h - child.baseline    (math axis to bottom)
     *
     * Find the maximum ascent and descent across ALL children:
     *   max_ascent  = max over all children of (child.baseline)
     *   max_descent = max over all children of (child.h - child.baseline)
     *
     * Then:
     *   row.w        = sum of all child widths + gaps
     *   row.h        = max_ascent + max_descent
     *   row.baseline = max_ascent
     *
     * During render_draw (pass 2), each child is placed at:
     *   child_y = row_y + max_ascent - child.baseline
     *
     * This guarantees that every child's math axis lands at the same
     * y-coordinate, so fractions, text, and scripts all align.
     * ================================================================== */
    case MATH_ROW:
    {
        int total_w     = 0;
        int max_ascent  = 0;
        int max_descent = 0;

        for (int i = 0; i < node->d.row.count; i++) {
            MathNode *child = node->d.row.children[i];
            if (!child) continue;

            render_layout(child);

            if (i > 0) total_w += ROW_GAP;
            total_w += child->layout.w;

            int asc  = child->layout.baseline;
            int desc = child->layout.h - child->layout.baseline;
            if (asc  > max_ascent)  max_ascent  = asc;
            if (desc > max_descent) max_descent = desc;
        }

        node->layout.w        = total_w;
        node->layout.h        = max_ascent + max_descent;
        node->layout.baseline  = max_ascent;
        break;
    }

    /* ==================================================================
     * MATH_INTEGRAL  /  MATH_SUMMATION  (unified bigop layout)
     * ==================================================================
     *       [upper limit]          ← optional, AUTO-DEMOTED to SMALL
     *      ⌠  or  Σ
     *      │       [body]
     *      ⌡  or  Σ
     *       [lower limit]          ← optional, AUTO-DEMOTED to SMALL
     *
     * sign_col = width of sign + limits (whichever is wider)
     * body goes to the right of sign_col + BIGOP_PAD
     * ================================================================== */
    case MATH_INTEGRAL:
    case MATH_SUMMATION:
    {
        /* ---- AUTO-DEMOTION of limits ---- */
        if (node->d.bigop.lower)
            node->d.bigop.lower->font_tier = FONT_SMALL;
        if (node->d.bigop.upper)
            node->d.bigop.upper->font_tier = FONT_SMALL;

        if (node->d.bigop.lower) render_layout(node->d.bigop.lower);
        if (node->d.bigop.upper) render_layout(node->d.bigop.upper);
        render_layout(node->d.bigop.body);

        LayoutInfo *body = &node->d.bigop.body->layout;

        int upper_h = node->d.bigop.upper
            ? node->d.bigop.upper->layout.h + LIMIT_PAD : 0;
        int lower_h = node->d.bigop.lower
            ? node->d.bigop.lower->layout.h + LIMIT_PAD : 0;

        /* Sign column: wide enough for the glyph AND the limits */
        int glyph_w = (node->type == MATH_INTEGRAL) ? INTEGRAL_W : SIGMA_W;
        int sign_col = glyph_w;
        if (node->d.bigop.upper)
            sign_col = imax(sign_col, node->d.bigop.upper->layout.w);
        if (node->d.bigop.lower)
            sign_col = imax(sign_col, node->d.bigop.lower->layout.w);

        int min_sign_h = (node->type == MATH_INTEGRAL)
                       ? tier_glyph_h(FONT_NORMAL) * 2
                       : SIGMA_H;
        int body_region_h = imax(body->h, min_sign_h);
        int total_h = upper_h + body_region_h + lower_h;

        node->layout.w = sign_col + BIGOP_PAD + body->w;
        node->layout.h = total_h;
        node->layout.baseline = upper_h + body->baseline;
        break;
    }

    /* ==================================================================
     * MATH_BRA  /  MATH_KET
     * ==================================================================
     * BRA:  ⟨ + pad + content + pad + |
     * KET:  | + pad + content + pad + ⟩
     * ================================================================== */
    case MATH_BRA:
    case MATH_KET:
    {
        render_layout(node->d.bracket.content);
        LayoutInfo *c = &node->d.bracket.content->layout;

        node->layout.w = BRACKET_W + BRACKET_PAD + c->w + BRACKET_PAD + BAR_W;
        node->layout.h = c->h + PAREN_VERT_PAD * 2;
        node->layout.baseline = PAREN_VERT_PAD + c->baseline;
        break;
    }

    /* ==================================================================
     * MATH_BRAKET:  ⟨ + pad + bra + pad + | + pad + ket + pad + ⟩
     * ================================================================== */
    case MATH_BRAKET:
    {
        render_layout(node->d.braket.bra);
        render_layout(node->d.braket.ket);

        LayoutInfo *b = &node->d.braket.bra->layout;
        LayoutInfo *k = &node->d.braket.ket->layout;
        int mh = imax(b->h, k->h);

        node->layout.w = BRACKET_W + BRACKET_PAD
                        + b->w     + BRACKET_PAD
                        + BAR_W    + BRACKET_PAD
                        + k->w     + BRACKET_PAD
                        + BRACKET_W;
        node->layout.h = mh + PAREN_VERT_PAD * 2;
        node->layout.baseline = PAREN_VERT_PAD + mh / 2;
        break;
    }

    /* ==================================================================
     * MATH_SANDWICH:  ⟨ + bra + | + op + | + ket + ⟩
     * ================================================================== */
    case MATH_SANDWICH:
    {
        render_layout(node->d.sandwich.bra);
        render_layout(node->d.sandwich.op);
        render_layout(node->d.sandwich.ket);

        LayoutInfo *b = &node->d.sandwich.bra->layout;
        LayoutInfo *o = &node->d.sandwich.op->layout;
        LayoutInfo *k = &node->d.sandwich.ket->layout;
        int mh = imax(b->h, imax(o->h, k->h));

        node->layout.w = BRACKET_W + BRACKET_PAD
                        + b->w     + BRACKET_PAD
                        + BAR_W    + BRACKET_PAD
                        + o->w     + BRACKET_PAD
                        + BAR_W    + BRACKET_PAD
                        + k->w     + BRACKET_PAD
                        + BRACKET_W;
        node->layout.h = mh + PAREN_VERT_PAD * 2;
        node->layout.baseline = PAREN_VERT_PAD + mh / 2;
        break;
    }

    /* ==================================================================
     * MATH_HAT / MATH_BAR
     * ================================================================== */
    case MATH_HAT:
    {
        render_layout(node->d.accent.child);
        LayoutInfo *c = &node->d.accent.child->layout;
        node->layout.w = c->w;
        node->layout.h = HAT_HEIGHT + c->h;
        node->layout.baseline = HAT_HEIGHT + c->baseline;
        break;
    }
    case MATH_BAR:
    {
        render_layout(node->d.accent.child);
        LayoutInfo *c = &node->d.accent.child->layout;
        node->layout.w = c->w;
        node->layout.h = BAR_HEIGHT + c->h;
        node->layout.baseline = BAR_HEIGHT + c->baseline;
        break;
    }

    /* ==================================================================
     * MATH_ARROW — fixed-size glyph
     * ================================================================== */
    case MATH_ARROW:
    {
        node->layout.w = ARROW_W;
        node->layout.h = tier_glyph_h(FONT_NORMAL);
        node->layout.baseline = node->layout.h / 2;
        break;
    }

    default:
        node->layout.w = node->layout.h = node->layout.baseline = 0;
        break;
    }
}


/* #########################################################################
 * §7: DRAWING HELPERS — DOUBLE-STROKED PRIMITIVES
 * #########################################################################
 *
 * The CG50's 396×224 LCD is slightly washed out and low-DPI.  Single-pixel
 * lines for brackets and operator glyphs are hard to read.  All helpers
 * below draw TWO parallel strokes (1 pixel apart) for visibility.
 *
 * These are called from render_draw() to avoid repeating bracket/sign
 * drawing code across BRA, KET, BRAKET, SANDWICH, INTEGRAL, SUMMATION.
 * ######################################################################### */

/* Left angle bracket ⟨ : two diagonals meeting at the leftmost point */
static void draw_angle_left(int x, int yt, int yb, int c)
{
    int my = (yt + yb) / 2;
    dline(x + BRACKET_W - 2, yt, x,     my, c);
    dline(x,     my, x + BRACKET_W - 2, yb, c);
    /* second stroke (1 px right) for double-width visibility */
    dline(x + BRACKET_W - 1, yt, x + 1, my, c);
    dline(x + 1, my, x + BRACKET_W - 1, yb, c);
}

/* Right angle bracket ⟩ */
static void draw_angle_right(int x, int yt, int yb, int c)
{
    int my = (yt + yb) / 2;
    dline(x,                 yt, x + BRACKET_W - 2, my, c);
    dline(x + BRACKET_W - 2, my, x,                 yb, c);
    dline(x + 1,             yt, x + BRACKET_W - 1, my, c);
    dline(x + BRACKET_W - 1, my, x + 1,             yb, c);
}

/* Vertical bar | (double-stroked) */
static void draw_vert_bar(int x, int yt, int yb, int c)
{
    dline(x,     yt, x,     yb, c);
    dline(x + 1, yt, x + 1, yb, c);
}

/* Left parenthesis ( with adaptive curvature:
 *   Short (< 16 px): straight vertical + endcap pixels
 *   Tall  (≥ 16 px): three-segment arc (curve, straight, curve) */
static void draw_paren_left(int x, int yt, int yb, int c)
{
    int h  = yb - yt;
    int cx = x + PAREN_PAD - 1;

    if (h < 16) {
        dline(cx, yt + 1, cx, yb - 1, c);
        dpixel(cx + 1, yt, c);
        dpixel(cx + 1, yb, c);
    } else {
        int q = h / 4;
        dline(cx + 2, yt,      cx,     yt + q,  c);   /* top curve    */
        dline(cx,     yt + q,  cx,     yb - q,  c);   /* middle shaft */
        dline(cx,     yb - q,  cx + 2, yb,      c);   /* bottom curve */
    }
}

/* Right parenthesis ) — mirror of left */
static void draw_paren_right(int x, int yt, int yb, int c)
{
    int h  = yb - yt;
    int cx = x;

    if (h < 16) {
        dline(cx, yt + 1, cx, yb - 1, c);
        dpixel(cx - 1, yt, c);
        dpixel(cx - 1, yb, c);
    } else {
        int q = h / 4;
        dline(cx - 2, yt,      cx,     yt + q,  c);
        dline(cx,     yt + q,  cx,     yb - q,  c);
        dline(cx,     yb - q,  cx - 2, yb,      c);
    }
}

/* Integral sign ∫ — stretched to fill [yt, yb].
 *
 *     ╮       top hook curves right
 *     ║       vertical shaft (double-stroked)
 *    ╰        bottom hook curves left
 */
static void draw_integral_sign(int x, int yt, int yb, int c)
{
    int cx   = x + INTEGRAL_W / 2;
    int st   = yt + 4;     /* shaft top    */
    int sb   = yb - 4;     /* shaft bottom */

    /* Top hook: 4 pixels curving right → down */
    dpixel(cx + 3, yt,     c);
    dpixel(cx + 2, yt + 1, c);
    dpixel(cx + 1, yt + 2, c);
    dpixel(cx,     yt + 3, c);

    /* Vertical shaft — double-stroked */
    dline(cx,     st, cx,     sb, c);
    dline(cx + 1, st, cx + 1, sb, c);

    /* Bottom hook: 4 pixels curving left → down */
    dpixel(cx,     yb - 3, c);
    dpixel(cx - 1, yb - 2, c);
    dpixel(cx - 2, yb - 1, c);
    dpixel(cx - 3, yb,     c);
}

/* Sigma sign Σ — drawn in a box [x, yt] to [x+SIGMA_W, yb].
 *
 *   ═══════   top bar
 *    ╲
 *      ╲      diagonals meeting at center (double-stroked)
 *      ╱
 *    ╱
 *   ═══════   bottom bar
 */
static void draw_sigma_sign(int x, int yt, int yb, int c)
{
    int w  = SIGMA_W - 2;
    int my = (yt + yb) / 2;

    /* Top bar (2 px thick) */
    drect(x, yt, x + w, yt + 1, c);

    /* Upper diagonal (double-stroked) */
    dline(x,     yt + 2,  x + w / 2,     my, c);
    dline(x + 1, yt + 2,  x + w / 2 + 1, my, c);

    /* Lower diagonal (double-stroked) */
    dline(x + w / 2,     my, x,     yb - 2, c);
    dline(x + w / 2 + 1, my, x + 1, yb - 2, c);

    /* Bottom bar (2 px thick) */
    drect(x, yb - 1, x + w, yb, c);
}


/* #########################################################################
 * §8: PASS 2 — render_draw() — TOP-DOWN VRAM BLITTING
 * #########################################################################
 *
 * Each node receives (x, y) = top-left corner of its bounding box.
 * It draws its own visual elements, computes child positions from
 * its LayoutInfo (set during pass 1), and recurses into children.
 *
 * Does NOT call dupdate().  The caller commits the framebuffer.
 * ######################################################################### */

void render_draw(const MathNode *node, int x, int y)
{
    if (!node) return;

    switch (node->type) {

    /* ==================================================================
     * LEAF NODES — draw text at (x, y)
     * ================================================================== */
    case MATH_TEXT:
    case MATH_NUMBER:
    case MATH_SYMBOL:
        dtext(x, y, COL_MATH_FG, node->d.leaf.text);
        break;

    /* ==================================================================
     * MATH_FRACTION — numerator, vinculum, denominator
     * ================================================================== */
    case MATH_FRACTION:
    {
        LayoutInfo *num = &node->d.frac.numerator->layout;
        LayoutInfo *den = &node->d.frac.denominator->layout;
        int fw = node->layout.w;

        /* Numerator: centered above the bar */
        render_draw(node->d.frac.numerator,
                    x + (fw - num->w) / 2, y);

        /* Vinculum: drawn at the baseline */
        int bar_y = y + num->h + FRAC_BAR_PAD;
        drect(x, bar_y, x + fw - 1, bar_y + FRAC_BAR_THICK - 1, COL_MATH_FG);

        /* Denominator: centered below the bar */
        render_draw(node->d.frac.denominator,
                    x + (fw - den->w) / 2,
                    bar_y + FRAC_BAR_THICK + FRAC_BAR_PAD);
        break;
    }

    /* ==================================================================
     * MATH_SUPERSCRIPT — script at top, base offset down
     * ================================================================== */
    case MATH_SUPERSCRIPT:
    {
        LayoutInfo *b = &node->d.script.base->layout;
        LayoutInfo *s = &node->d.script.script->layout;

        int base_y = s->h - SCRIPT_RISE;
        if (base_y < 0) base_y = 0;

        render_draw(node->d.script.base,   x,                      y + base_y);
        render_draw(node->d.script.script,  x + b->w + SCRIPT_GAP, y);
        break;
    }

    /* ==================================================================
     * MATH_SUBSCRIPT — base at top, script drops below
     * ================================================================== */
    case MATH_SUBSCRIPT:
    {
        LayoutInfo *b = &node->d.script.base->layout;
        int script_y = b->h - SCRIPT_DROP;

        render_draw(node->d.script.base,   x,                      y);
        render_draw(node->d.script.script,  x + b->w + SCRIPT_GAP, y + script_y);
        break;
    }

    /* ==================================================================
     * MATH_SQRT — radical hook + overbar + radicand
     * ================================================================== */
    case MATH_SQRT:
    {
        LayoutInfo *r = &node->d.sqrt.radicand->layout;
        int hb = y + node->layout.h - 1;           /* hook bottom y  */
        int ht = y + SQRT_TOP_PAD - 1;             /* overbar y      */

        /* Small lead-in tick */
        dline(x, y + node->layout.h * 2 / 3, x + 3, hb, COL_MATH_FG);
        /* Main diagonal (double-stroked) */
        dline(x + 3, hb, x + SQRT_HOOK_W - 1, ht, COL_MATH_FG);
        dline(x + 4, hb, x + SQRT_HOOK_W,     ht, COL_MATH_FG);
        /* Overbar */
        int bar_end = x + SQRT_HOOK_W + r->w + SQRT_RIGHT_PAD - 1;
        dline(x + SQRT_HOOK_W - 1, ht, bar_end, ht, COL_MATH_FG);
        /* Right tick */
        dline(bar_end, ht, bar_end, ht + 3, COL_MATH_FG);
        /* Radicand */
        render_draw(node->d.sqrt.radicand, x + SQRT_HOOK_W, y + SQRT_TOP_PAD);
        break;
    }

    /* ==================================================================
     * MATH_PAREN — scaled ( inner )
     * ================================================================== */
    case MATH_PAREN:
    {
        LayoutInfo *c = &node->d.paren.inner->layout;
        int bt = y;
        int bb = y + node->layout.h - 1;

        draw_paren_left(x, bt, bb, COL_MATH_FG);
        render_draw(node->d.paren.inner, x + PAREN_PAD, y + PAREN_VERT_PAD);
        draw_paren_right(x + PAREN_PAD + c->w + PAREN_PAD - 1,
                         bt, bb, COL_MATH_FG);
        break;
    }

    /* ==================================================================
     * MATH_ROW — baseline-aligned horizontal sequence
     * ==================================================================
     * Each child is placed so that:
     *   child_y + child.baseline == y + row.baseline
     * ⟹ child_y = y + row.baseline - child.baseline
     * ================================================================== */
    case MATH_ROW:
    {
        int cx    = x;
        int rowbl = node->layout.baseline;

        for (int i = 0; i < node->d.row.count; i++) {
            MathNode *child = node->d.row.children[i];
            if (!child) continue;

            int child_y = y + rowbl - child->layout.baseline;
            render_draw(child, cx, child_y);
            cx += child->layout.w + ROW_GAP;
        }
        break;
    }

    /* ==================================================================
     * MATH_INTEGRAL — ∫ sign + limits + body
     * ==================================================================
     * Layout:  [upper limit]   ← centered in sign column
     *          ∫ sign          ← spans body region
     *          [lower limit]   ← centered in sign column
     *                [body]    ← to the right, vertically centered
     * ================================================================== */
    case MATH_INTEGRAL:
    {
        LayoutInfo *body = &node->d.bigop.body->layout;

        int upper_h = node->d.bigop.upper
            ? node->d.bigop.upper->layout.h + LIMIT_PAD : 0;

        int min_sign_h = tier_glyph_h(FONT_NORMAL) * 2;
        int body_rh    = imax(body->h, min_sign_h);

        int sign_col = INTEGRAL_W;
        if (node->d.bigop.upper)
            sign_col = imax(sign_col, node->d.bigop.upper->layout.w);
        if (node->d.bigop.lower)
            sign_col = imax(sign_col, node->d.bigop.lower->layout.w);

        /* Upper limit */
        if (node->d.bigop.upper) {
            int ux = x + (sign_col - node->d.bigop.upper->layout.w) / 2;
            render_draw(node->d.bigop.upper, ux, y);
        }

        /* Integral sign glyph */
        int st = y + upper_h;
        int sb = y + upper_h + body_rh - 1;
        int sx = x + (sign_col - INTEGRAL_W) / 2;
        draw_integral_sign(sx, st, sb, COL_MATH_FG);

        /* Lower limit */
        if (node->d.bigop.lower) {
            int lx = x + (sign_col - node->d.bigop.lower->layout.w) / 2;
            int ly = y + upper_h + body_rh + LIMIT_PAD;
            render_draw(node->d.bigop.lower, lx, ly);
        }

        /* Body */
        int bx = x + sign_col + BIGOP_PAD;
        int by = y + upper_h + (body_rh - body->h) / 2;
        render_draw(node->d.bigop.body, bx, by);
        break;
    }

    /* ==================================================================
     * MATH_SUMMATION — Σ sign + limits + body  (same structure)
     * ================================================================== */
    case MATH_SUMMATION:
    {
        LayoutInfo *body = &node->d.bigop.body->layout;

        int upper_h = node->d.bigop.upper
            ? node->d.bigop.upper->layout.h + LIMIT_PAD : 0;

        int body_rh = imax(body->h, SIGMA_H);

        int sign_col = SIGMA_W;
        if (node->d.bigop.upper)
            sign_col = imax(sign_col, node->d.bigop.upper->layout.w);
        if (node->d.bigop.lower)
            sign_col = imax(sign_col, node->d.bigop.lower->layout.w);

        /* Upper limit */
        if (node->d.bigop.upper) {
            int ux = x + (sign_col - node->d.bigop.upper->layout.w) / 2;
            render_draw(node->d.bigop.upper, ux, y);
        }

        /* Sigma glyph */
        int st = y + upper_h;
        int sb = y + upper_h + body_rh - 1;
        int sx = x + (sign_col - SIGMA_W) / 2;
        draw_sigma_sign(sx, st, sb, COL_MATH_FG);

        /* Lower limit */
        if (node->d.bigop.lower) {
            int lx = x + (sign_col - node->d.bigop.lower->layout.w) / 2;
            int ly = y + upper_h + body_rh + LIMIT_PAD;
            render_draw(node->d.bigop.lower, lx, ly);
        }

        /* Body */
        int bx = x + sign_col + BIGOP_PAD;
        int by = y + upper_h + (body_rh - body->h) / 2;
        render_draw(node->d.bigop.body, bx, by);
        break;
    }

    /* ==================================================================
     * MATH_BRA:  ⟨ content |
     * ================================================================== */
    case MATH_BRA:
    {
        int yt = y, yb = y + node->layout.h - 1;
        int cx = x;

        draw_angle_left(cx, yt, yb, COL_MATH_FG);
        cx += BRACKET_W + BRACKET_PAD;

        render_draw(node->d.bracket.content, cx, y + PAREN_VERT_PAD);
        cx += node->d.bracket.content->layout.w + BRACKET_PAD;

        draw_vert_bar(cx, yt, yb, COL_MATH_FG);
        break;
    }

    /* ==================================================================
     * MATH_KET:  | content ⟩
     * ================================================================== */
    case MATH_KET:
    {
        int yt = y, yb = y + node->layout.h - 1;
        int cx = x;

        draw_vert_bar(cx, yt, yb, COL_MATH_FG);
        cx += BAR_W + BRACKET_PAD;

        render_draw(node->d.bracket.content, cx, y + PAREN_VERT_PAD);
        cx += node->d.bracket.content->layout.w + BRACKET_PAD;

        draw_angle_right(cx, yt, yb, COL_MATH_FG);
        break;
    }

    /* ==================================================================
     * MATH_BRAKET:  ⟨ bra | ket ⟩
     * ================================================================== */
    case MATH_BRAKET:
    {
        int yt = y, yb = y + node->layout.h - 1;
        int cx = x;

        draw_angle_left(cx, yt, yb, COL_MATH_FG);
        cx += BRACKET_W + BRACKET_PAD;

        render_draw(node->d.braket.bra, cx, y + PAREN_VERT_PAD);
        cx += node->d.braket.bra->layout.w + BRACKET_PAD;

        draw_vert_bar(cx, yt, yb, COL_MATH_FG);
        cx += BAR_W + BRACKET_PAD;

        render_draw(node->d.braket.ket, cx, y + PAREN_VERT_PAD);
        cx += node->d.braket.ket->layout.w + BRACKET_PAD;

        draw_angle_right(cx, yt, yb, COL_MATH_FG);
        break;
    }

    /* ==================================================================
     * MATH_SANDWICH:  ⟨ bra | op | ket ⟩
     * ================================================================== */
    case MATH_SANDWICH:
    {
        int yt = y, yb = y + node->layout.h - 1;
        int cx = x;

        draw_angle_left(cx, yt, yb, COL_MATH_FG);
        cx += BRACKET_W + BRACKET_PAD;

        render_draw(node->d.sandwich.bra, cx, y + PAREN_VERT_PAD);
        cx += node->d.sandwich.bra->layout.w + BRACKET_PAD;

        draw_vert_bar(cx, yt, yb, COL_MATH_FG);
        cx += BAR_W + BRACKET_PAD;

        render_draw(node->d.sandwich.op, cx, y + PAREN_VERT_PAD);
        cx += node->d.sandwich.op->layout.w + BRACKET_PAD;

        draw_vert_bar(cx, yt, yb, COL_MATH_FG);
        cx += BAR_W + BRACKET_PAD;

        render_draw(node->d.sandwich.ket, cx, y + PAREN_VERT_PAD);
        cx += node->d.sandwich.ket->layout.w + BRACKET_PAD;

        draw_angle_right(cx, yt, yb, COL_MATH_FG);
        break;
    }

    /* ==================================================================
     * MATH_HAT — circumflex ^ above child
     * ================================================================== */
    case MATH_HAT:
    {
        LayoutInfo *c = &node->d.accent.child->layout;
        int px = x + c->w / 2;   /* peak of the circumflex */

        dline(px - 4, y + HAT_HEIGHT - 1,  px,     y, COL_MATH_FG);
        dline(px,     y,                    px + 4, y + HAT_HEIGHT - 1, COL_MATH_FG);

        render_draw(node->d.accent.child, x, y + HAT_HEIGHT);
        break;
    }

    /* ==================================================================
     * MATH_BAR — overline above child (2 px thick)
     * ================================================================== */
    case MATH_BAR:
    {
        LayoutInfo *c = &node->d.accent.child->layout;
        drect(x, y, x + c->w - 1, y + 1, COL_MATH_FG);
        render_draw(node->d.accent.child, x, y + BAR_HEIGHT);
        break;
    }

    /* ==================================================================
     * MATH_ARROW — right arrow → (double-stroked shaft + barbs)
     * ================================================================== */
    case MATH_ARROW:
    {
        int my = y + node->layout.h / 2;
        int ex = x + ARROW_W - 1;

        /* Double-stroked shaft */
        dline(x, my,     ex, my,     COL_MATH_FG);
        dline(x, my + 1, ex, my + 1, COL_MATH_FG);

        /* Arrowhead barbs */
        dline(ex - ARROW_HEAD, my - ARROW_HEAD,     ex, my,     COL_MATH_FG);
        dline(ex - ARROW_HEAD, my + ARROW_HEAD + 1, ex, my + 1, COL_MATH_FG);
        break;
    }

    default:
        break;
    }
}


/* #########################################################################
 * §8.5: TIER OVERRIDE WALKER (added in v4 for cut-off equation fix)
 * #########################################################################
 *
 * Recursively sets `font_tier` on every node in a subtree.  Used by the
 * topic renderer when an equation's bounding box overflows the screen
 * width: demote NORMAL -> SMALL, then call render_layout() again to get
 * the smaller bbox before drawing.
 *
 * This walker mirrors the structural recursion in render_layout(): for
 * each compound node type we descend into ALL relevant children so even
 * deeply nested subtrees pick up the tier change.
 * ######################################################################### */

void render_force_tier(MathNode *node, FontTier tier)
{
    if (!node) return;
    node->font_tier = tier;

    switch (node->type) {
    case MATH_TEXT:
    case MATH_NUMBER:
    case MATH_SYMBOL:
    case MATH_ARROW:
        /* leaves -- nothing to recurse into */
        break;

    case MATH_FRACTION:
        render_force_tier(node->d.frac.numerator,   tier);
        render_force_tier(node->d.frac.denominator, tier);
        break;

    case MATH_SUPERSCRIPT:
    case MATH_SUBSCRIPT:
        render_force_tier(node->d.script.base,   tier);
        render_force_tier(node->d.script.script, tier);
        break;

    case MATH_SQRT:
        render_force_tier(node->d.sqrt.radicand, tier);
        break;

    case MATH_PAREN:
        render_force_tier(node->d.paren.inner, tier);
        break;

    case MATH_ROW:
        for (int i = 0; i < node->d.row.count; i++)
            render_force_tier(node->d.row.children[i], tier);
        break;

    case MATH_INTEGRAL:
    case MATH_SUMMATION:
        render_force_tier(node->d.bigop.lower, tier);
        render_force_tier(node->d.bigop.upper, tier);
        render_force_tier(node->d.bigop.body,  tier);
        break;

    case MATH_BRA:
    case MATH_KET:
        render_force_tier(node->d.bracket.content, tier);
        break;

    case MATH_BRAKET:
        render_force_tier(node->d.braket.bra, tier);
        render_force_tier(node->d.braket.ket, tier);
        break;

    case MATH_SANDWICH:
        render_force_tier(node->d.sandwich.bra, tier);
        render_force_tier(node->d.sandwich.op,  tier);
        render_force_tier(node->d.sandwich.ket, tier);
        break;

    case MATH_HAT:
    case MATH_BAR:
        render_force_tier(node->d.accent.child, tier);
        break;

    default:
        break;
    }
}


/* #########################################################################
 * §9: DEBUG UTILITIES
 * ######################################################################### */

static const char *type_names[MATH_NODE_TYPE_COUNT] = {
    "TEXT",        "NUMBER",      "SYMBOL",
    "FRACTION",    "SUPERSCRIPT", "SUBSCRIPT",
    "SQRT",        "PAREN",       "ROW",
    "INTEGRAL",    "SUMMATION",
    "BRA",         "KET",         "BRAKET",    "SANDWICH",
    "HAT",         "BAR",         "ARROW"
};

const char *render_type_name(MathNodeType type)
{
    if (type >= 0 && type < MATH_NODE_TYPE_COUNT)
        return type_names[type];
    return "UNKNOWN";
}
