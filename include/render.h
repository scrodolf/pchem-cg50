/* ==========================================================================
 * render.h — 2D Math Rendering Engine: Public Interface
 * ==========================================================================
 *
 * PURPOSE
 * -------
 * This header declares every type, constant, and function needed to build
 * mathematical expression trees and render them to the fx-CG50's 396×224
 * pixel VRAM framebuffer.  The implementation lives in render.c.
 *
 * IMPLEMENTATION PLAN (executed in render.c, documented here for readers)
 * ======================================================================
 *
 *  Step 1 — FONT MODEL
 *     Map the fx-CG50's two physical OS font sizes (24px, 18px) into
 *     three logical tiers: LARGE (big operators), NORMAL (body text),
 *     SMALL (scripts/limits).  Store the tier directly on each MathNode.
 *
 *  Step 2 — SYMBOL TABLE
 *     Build a 42-entry lookup table that maps human-readable names
 *     ("psi", "Delta") to the OS's proprietary 2-byte multi-byte
 *     character codes ("\xE5\xB8", "\xE5\x84"), with pre-measured
 *     pixel widths so layout never needs to query the font engine.
 *
 *  Step 3 — VISUAL VOCABULARY
 *     Define an enum of 18 distinct math structures: 3 leaf types,
 *     6 structural types, and 9 physics-specific types (including
 *     Dirac bra-ket notation and big operators).
 *
 *  Step 4 — AST DATA MODEL
 *     Define MathNode as a tagged union.  Each node carries its type
 *     tag, a LayoutInfo struct (w, h, baseline), a FontTier, and a
 *     union of type-specific payloads.  INTEGRAL and SUMMATION share
 *     a single "bigop" union member since their children are identical.
 *
 *  Step 5 — MEMORY MODEL
 *     Use a static pool of 256 MathNode slots (~22 KB).  Bump-pointer
 *     allocation, O(1) bulk reset per screen.  No malloc, no free,
 *     no fragmentation on the MMU-less SH4 CPU.
 *
 *  Step 6 — CONSTRUCTORS
 *     Provide ~20 convenience functions (math_text, math_fraction, ...)
 *     that allocate a node, set its fields, and return it.
 *
 *  Step 7 — TWO-PASS RENDERER (Eigenmath architecture)
 *     render_layout(): bottom-up recursive walk.  Each node computes
 *       its bounding box (w, h, baseline) from its children's layouts.
 *       Automatically demotes font tier for scripts and bigop limits.
 *     render_draw(): top-down recursive walk.  Each node draws its
 *       visual elements and positions its children within its bbox.
 *       Uses double-stroked line primitives for CG50 LCD readability.
 *
 * BASELINE ALIGNMENT
 * ------------------
 * Every node has a "baseline" — pixels from the top of its bounding box
 * to the "math axis" where fraction bars sit.  When a ROW places children
 * side-by-side, it aligns them by baseline (not by top edge), which is
 * what makes  E = n²h²/8mL²  render correctly instead of misaligned.
 *
 *     ┌──────────────┐  ← top of bounding box (y = 0)
 *     │  ascent part │
 *     │══════════════│  ← baseline (y = baseline)
 *     │ descent part │
 *     └──────────────┘  ← bottom (y = h)
 *
 * DIRAC BRA-KET NOTATION
 * ----------------------
 * Structures like ⟨φ|Ĥ|ψ⟩ are first-class AST nodes.  Angle brackets
 * and vertical bars are DRAWN as scaled line primitives (not stored as
 * text), so they stretch to match the content height.
 * ========================================================================== */

#ifndef RENDER_H
#define RENDER_H

#include "pchem.h"
#include <stdint.h>

/* =========================================================================
 *  §1  FONT TIER SYSTEM
 * =========================================================================
 *
 *  The fx-CG50 OS contains built-in bitmap fonts at two pixel sizes,
 *  accessible through gint's dtext() / dfont() API:
 *
 *    24px font  — ~13 px wide for ASCII.  Used for titles and the large
 *                 drawn forms of Σ and ∫.
 *    18px font  — ~ 9 px wide for ASCII, ~10 px for multi-byte Greek
 *                 glyphs.  Contains α β γ … ω, Δ Σ Π Ψ Ω, ± × ÷ ≤ ≥
 *                 ≠ ∞ →.  This is the default body text font.
 *
 *  We map these two physical fonts into three logical tiers:
 *
 *    FONT_LARGE  (24 px)  — big operator symbols, display equations
 *    FONT_NORMAL (18 px)  — body text, standard math variables
 *    FONT_SMALL  (18 px)  — scripts, limits.  Same physical font but
 *                           tracked separately so a future custom 12 px
 *                           bitmap font could render them truly smaller.
 *
 *  The FontTier is stored on every MathNode.  render_layout() reads it
 *  to pick the right glyph metrics, and automatically demotes children
 *  of SUPERSCRIPT, SUBSCRIPT, and bigop limits before recursing.
 * ========================================================================= */
typedef enum {
    FONT_LARGE  = 0,
    FONT_NORMAL = 1,
    FONT_SMALL  = 2
} FontTier;

/* =========================================================================
 *  §2  SYMBOL TABLE
 * =========================================================================
 *
 *  The fx-CG50 OS encodes special characters as 2-byte sequences:
 *    byte 0 = lead byte (0xE5, 0xE6, or 0xE7)
 *    byte 1 = glyph index within that block
 *
 *  Source:  WikiPrizm  (prizm.cemetech.net)  and Cemetech forum thread
 *           viewtopic.php?t=18603 (gint OS font glyph discussion).
 *
 *  We expose a name→bytes lookup so equation code stays readable:
 *
 *    math_symbol("psi")   →  node whose text = "\xE5\xB8" (ψ)
 *    math_symbol("Delta") →  node whose text = "\xE5\x84" (Δ)
 *
 *  Each entry also stores a pre-measured pixel width at 18 px so that
 *  render_layout() never needs to call dsize() at layout time.
 * ========================================================================= */

#define MAX_SYMBOLS  64

typedef struct {
    const char *name;       /* human-readable key, e.g. "alpha"            */
    const char *bytes;      /* OS 2-byte sequence,  e.g. "\xE5\xA0"       */
    int         width_px;   /* pixel width at FONT_NORMAL (18 px font)     */
} SymbolEntry;

/* Initialize the 42-entry table.  Call once from main() at startup. */
void sym_table_init(void);

/* Look up a name → OS byte string.  Returns the name itself as fallback. */
const char *sym(const char *name);

/* Look up a name → pixel width.  Returns strlen(name)*9 as fallback. */
int sym_width(const char *name);

/* =========================================================================
 *  §3  NODE TYPE ENUM  (18 types)
 * =========================================================================
 *  Ordered: 3 leaves → 6 structural → 9 physics-specific.
 * ========================================================================= */
typedef enum {
    /* ---- Leaf nodes (no children) ---- */
    MATH_TEXT,            /*  0  plain text / variable name                */
    MATH_NUMBER,          /*  1  numeric literal                           */
    MATH_SYMBOL,          /*  2  named symbol from sym() table             */

    /* ---- Structural nodes ---- */
    MATH_FRACTION,        /*  3  numerator / denominator + vinculum        */
    MATH_SUPERSCRIPT,     /*  4  base ^ exponent                          */
    MATH_SUBSCRIPT,       /*  5  base _ subscript                         */
    MATH_SQRT,            /*  6  radical hook + overbar + radicand         */
    MATH_PAREN,           /*  7  scaled ( inner )                         */
    MATH_ROW,             /*  8  baseline-aligned horizontal sequence     */

    /* ---- Physics-specific ---- */
    MATH_INTEGRAL,        /*  9  ∫ + optional limits + body               */
    MATH_SUMMATION,       /* 10  Σ + optional limits + body               */
    MATH_BRA,             /* 11  ⟨ content |                              */
    MATH_KET,             /* 12  | content ⟩                              */
    MATH_BRAKET,          /* 13  ⟨ left | right ⟩                         */
    MATH_SANDWICH,        /* 14  ⟨ bra | op | ket ⟩                       */
    MATH_HAT,             /* 15  circumflex ˆ over child                  */
    MATH_BAR,             /* 16  overline ¯ over child                    */
    MATH_ARROW,           /* 17  right arrow →                            */

    MATH_NODE_TYPE_COUNT  /* sentinel — must be last                      */
} MathNodeType;

/* =========================================================================
 *  §4  AST NODE STRUCTURE
 * =========================================================================
 *
 *  MathNode is a tagged union.  The `type` field selects which member
 *  of the `d` union is active.
 *
 *  KEY DESIGN DECISION:  INTEGRAL (type 9) and SUMMATION (type 10) share
 *  the same union member `d.bigop` because their child structure (lower
 *  limit, upper limit, body) is identical.  Only the drawn glyph differs.
 *
 *  sizeof(MathNode) ≈ 88 bytes.  Pool of 256 = ~22 KB.
 * ========================================================================= */

typedef struct MathNode MathNode;   /* forward declaration */

typedef struct {
    int16_t w;          /* bounding box width  (pixels)                    */
    int16_t h;          /* bounding box height (pixels)                    */
    int16_t baseline;   /* pixels from top edge to math axis               */
} LayoutInfo;

#define MAX_ROW_CHILDREN  16
#define MAX_TEXT_LEN      32

struct MathNode {
    MathNodeType type;          /* discriminant tag                         */
    LayoutInfo   layout;        /* filled by render_layout()               */
    FontTier     font_tier;     /* which logical font size to use          */

    union {
        /* MATH_TEXT, MATH_NUMBER, MATH_SYMBOL  (leaf) */
        struct { char text[MAX_TEXT_LEN]; } leaf;

        /* MATH_FRACTION */
        struct { MathNode *numerator; MathNode *denominator; } frac;

        /* MATH_SUPERSCRIPT, MATH_SUBSCRIPT */
        struct { MathNode *base; MathNode *script; } script;

        /* MATH_SQRT */
        struct { MathNode *radicand; } sqrt;

        /* MATH_PAREN */
        struct { MathNode *inner; } paren;

        /* MATH_ROW */
        struct { MathNode *children[MAX_ROW_CHILDREN]; int count; } row;

        /* MATH_INTEGRAL, MATH_SUMMATION  (unified bigop) */
        struct { MathNode *lower; MathNode *upper; MathNode *body; } bigop;

        /* MATH_BRA, MATH_KET */
        struct { MathNode *content; } bracket;

        /* MATH_BRAKET */
        struct { MathNode *bra; MathNode *ket; } braket;

        /* MATH_SANDWICH */
        struct { MathNode *bra; MathNode *op; MathNode *ket; } sandwich;

        /* MATH_HAT, MATH_BAR */
        struct { MathNode *child; } accent;

    } d;  /* short name: node->d.frac.numerator reads clearly enough */
};

/* =========================================================================
 *  §5  POOL ALLOCATOR
 * =========================================================================
 *  Static array of 256 nodes.  Bump pointer.  O(1) bulk reset.
 *  No malloc.  No fragmentation.  Deterministic 22 KB footprint.
 * ========================================================================= */

#define MATH_NODE_POOL_SIZE  256

void      render_pool_reset(void);   /* invalidate all nodes in O(1)       */
MathNode *render_node_alloc(void);   /* allocate one zeroed node           */
int       render_pool_used(void);    /* nodes allocated since last reset   */

/* =========================================================================
 *  §6  CONVENIENCE CONSTRUCTORS
 * =========================================================================
 *  Each allocates a node, sets type + payload, returns pointer.
 *  NULL return means pool exhausted (should never happen).
 *
 *  Example — building  E_n = n²h² / 8mL²  :
 *
 *    math_row((MathNode*[]){
 *        math_subscript(math_text("E"), math_text("n")),
 *        math_text(" = "),
 *        math_fraction(
 *            math_row((MathNode*[]){
 *                math_superscript(math_text("n"), math_number("2")),
 *                math_superscript(math_text("h"), math_number("2")),
 *            }, 2),
 *            math_row((MathNode*[]){
 *                math_number("8"), math_text("m"),
 *                math_superscript(math_text("L"), math_number("2")),
 *            }, 3)
 *        ),
 *    }, 3);
 * ========================================================================= */

/* Leaf constructors */
MathNode *math_text(const char *str);
MathNode *math_number(const char *str);
MathNode *math_symbol(const char *name);         /* sym() lookup           */
MathNode *math_text_small(const char *str);      /* force FONT_SMALL       */
MathNode *math_number_small(const char *str);    /* force FONT_SMALL       */

/* Structural constructors */
MathNode *math_fraction(MathNode *num, MathNode *den);
MathNode *math_superscript(MathNode *base, MathNode *exp);
MathNode *math_subscript(MathNode *base, MathNode *sub);
MathNode *math_sqrt(MathNode *radicand);
MathNode *math_paren(MathNode *inner);
MathNode *math_row(MathNode **children, int count);

/* Big-operator constructors  (both use d.bigop) */
MathNode *math_integral(MathNode *lo, MathNode *hi, MathNode *body);
MathNode *math_summation(MathNode *lo, MathNode *hi, MathNode *body);

/* Dirac notation constructors */
MathNode *math_bra(MathNode *content);
MathNode *math_ket(MathNode *content);
MathNode *math_braket(MathNode *bra_label, MathNode *ket_label);
MathNode *math_sandwich(MathNode *bra, MathNode *op, MathNode *ket);

/* Accent constructors */
MathNode *math_hat(MathNode *child);
MathNode *math_bar(MathNode *child);

/* Miscellaneous */
MathNode *math_arrow(void);

/* =========================================================================
 *  §7  TWO-PASS RENDERING API
 * =========================================================================
 *  Protocol:
 *    1.  Build AST with constructors above.
 *    2.  render_layout(root)     — fills LayoutInfo on every node.
 *    3.  render_draw(root, x, y) — blits expression to VRAM at (x,y).
 *    4.  dupdate()               — caller commits framebuffer to LCD.
 *
 *  render_layout() walks BOTTOM-UP:  children first, then parent.
 *  render_draw()   walks TOP-DOWN:   parent draws, then positions children.
 *
 *  Neither function calls dupdate().  The caller does that once after
 *  drawing all expressions, producing a flicker-free frame.
 * ========================================================================= */

void render_layout(MathNode *node);
void render_draw(const MathNode *node, int x, int y);

/* Debug utility: return human-readable name for a node type */
const char *render_type_name(MathNodeType type);

#endif /* RENDER_H */
