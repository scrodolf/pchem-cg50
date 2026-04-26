/* ==========================================================================
 * render.h — 2D Math Rendering Engine: Public Interface
 * ==========================================================================
 *
 * PURPOSE
 * -------
 * This header declares every type, constant, and function needed to build
 * mathematical expression trees and render them to the fx-CG50's 396x224
 * pixel VRAM framebuffer.  The implementation lives in render.c.
 *
 * IMPLEMENTATION PLAN (executed in render.c, documented here for readers)
 * ======================================================================
 *
 *  Step 1 - FONT MODEL
 *     Map the fx-CG50's two physical OS font sizes (24px, 18px) into
 *     three logical tiers: LARGE (big operators), NORMAL (body text),
 *     SMALL (scripts/limits).  Store the tier directly on each MathNode.
 *
 *  Step 2 - SYMBOL TABLE
 *     Build a 42-entry lookup table that maps human-readable names
 *     ("psi", "Delta") to the OS's proprietary 2-byte multi-byte
 *     character codes ("\xE5\xB8", "\xE5\x84"), with pre-measured
 *     pixel widths so layout never needs to query the font engine.
 *
 *  Step 3 - VISUAL VOCABULARY
 *     Define an enum of 18 distinct math structures: 3 leaf types,
 *     6 structural types, and 9 physics-specific types (including
 *     Dirac bra-ket notation and big operators).
 *
 *  Step 4 - AST DATA MODEL
 *     Define MathNode as a tagged union.  Each node carries its type
 *     tag, a LayoutInfo struct (w, h, baseline), a FontTier, and a
 *     union of type-specific payloads.  INTEGRAL and SUMMATION share
 *     a single "bigop" union member since their children are identical.
 *
 *  Step 5 - MEMORY MODEL
 *     Use a static pool of 256 MathNode slots (~22 KB).  Bump-pointer
 *     allocation, O(1) bulk reset per screen.  No malloc, no free,
 *     no fragmentation on the MMU-less SH4 CPU.
 *
 *  Step 6 - CONSTRUCTORS
 *     Provide ~20 convenience functions (math_text, math_fraction, ...)
 *     that allocate a node, set its fields, and return it.
 *
 *  Step 7 - TWO-PASS RENDERER (Eigenmath architecture)
 *     render_layout(): bottom-up recursive walk.  Each node computes
 *       its bounding box (w, h, baseline) from its children's layouts.
 *       Automatically demotes font tier for scripts and bigop limits.
 *     render_draw(): top-down recursive walk.  Each node draws its
 *       visual elements and positions its children within its bbox.
 *       Uses double-stroked line primitives for CG50 LCD readability.
 * ========================================================================== */

#ifndef RENDER_H
#define RENDER_H

#include "pchem.h"
#include <stdint.h>

/* =========================================================================
 *  §1  FONT TIER SYSTEM
 * ========================================================================= */
typedef enum {
    FONT_LARGE  = 0,
    FONT_NORMAL = 1,
    FONT_SMALL  = 2
} FontTier;

/* =========================================================================
 *  §2  SYMBOL TABLE
 * ========================================================================= */

#define MAX_SYMBOLS  64

typedef struct {
    const char *name;       /* human-readable key, e.g. "alpha"            */
    const char *bytes;      /* OS 2-byte sequence,  e.g. "\xE5\xA0"        */
    int         width_px;   /* pixel width at FONT_NORMAL (18 px font)     */
} SymbolEntry;

/* Initialize the symbol table.  Call once from main() at startup. */
void sym_table_init(void);

/* Look up a name -> OS byte string.  Returns the name itself as fallback. */
const char *sym(const char *name);

/* Look up a name -> pixel width.  Returns strlen(name)*9 as fallback. */
int sym_width(const char *name);

/* =========================================================================
 *  §3  NODE TYPE ENUM  (18 types)
 * ========================================================================= */
typedef enum {
    MATH_TEXT,            /*  0  plain text / variable name                */
    MATH_NUMBER,          /*  1  numeric literal                           */
    MATH_SYMBOL,          /*  2  named symbol from sym() table             */

    MATH_FRACTION,        /*  3  numerator / denominator + vinculum        */
    MATH_SUPERSCRIPT,     /*  4  base ^ exponent                           */
    MATH_SUBSCRIPT,       /*  5  base _ subscript                          */
    MATH_SQRT,            /*  6  radical hook + overbar + radicand         */
    MATH_PAREN,           /*  7  scaled ( inner )                          */
    MATH_ROW,             /*  8  baseline-aligned horizontal sequence      */

    MATH_INTEGRAL,        /*  9  integral + optional limits + body         */
    MATH_SUMMATION,       /* 10  sigma + optional limits + body            */
    MATH_BRA,             /* 11  <| content |                              */
    MATH_KET,             /* 12  | content |>                              */
    MATH_BRAKET,          /* 13  < left | right >                          */
    MATH_SANDWICH,        /* 14  < bra | op | ket >                        */
    MATH_HAT,             /* 15  circumflex over child                     */
    MATH_BAR,             /* 16  overline over child                       */
    MATH_ARROW,           /* 17  right arrow                               */

    MATH_NODE_TYPE_COUNT
} MathNodeType;

/* =========================================================================
 *  §4  AST NODE STRUCTURE
 * ========================================================================= */

typedef struct MathNode MathNode;   /* forward declaration */

typedef struct {
    int16_t w;
    int16_t h;
    int16_t baseline;
} LayoutInfo;

#define MAX_ROW_CHILDREN  24
#define MAX_TEXT_LEN      32

struct MathNode {
    MathNodeType type;
    LayoutInfo   layout;
    FontTier     font_tier;

    union {
        struct { char text[MAX_TEXT_LEN]; } leaf;
        struct { MathNode *numerator; MathNode *denominator; } frac;
        struct { MathNode *base; MathNode *script; } script;
        struct { MathNode *radicand; } sqrt;
        struct { MathNode *inner; } paren;
        struct { MathNode *children[MAX_ROW_CHILDREN]; int count; } row;
        struct { MathNode *lower; MathNode *upper; MathNode *body; } bigop;
        struct { MathNode *content; } bracket;
        struct { MathNode *bra; MathNode *ket; } braket;
        struct { MathNode *bra; MathNode *op; MathNode *ket; } sandwich;
        struct { MathNode *child; } accent;
    } d;
};

/* =========================================================================
 *  §5  POOL ALLOCATOR
 * ========================================================================= */

#define MATH_NODE_POOL_SIZE  512

void      render_pool_reset(void);
MathNode *render_node_alloc(void);
int       render_pool_used(void);

/* =========================================================================
 *  §6  CONVENIENCE CONSTRUCTORS
 * ========================================================================= */

/* Leaf constructors */
MathNode *math_text(const char *str);
MathNode *math_number(const char *str);
MathNode *math_symbol(const char *name);
MathNode *math_text_small(const char *str);
MathNode *math_number_small(const char *str);

/* Structural constructors */
MathNode *math_fraction(MathNode *num, MathNode *den);
MathNode *math_superscript(MathNode *base, MathNode *exp);
MathNode *math_subscript(MathNode *base, MathNode *sub);
MathNode *math_sqrt(MathNode *radicand);
MathNode *math_paren(MathNode *inner);
MathNode *math_row(MathNode **children, int count);

/* Big-operator constructors */
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
 * ========================================================================= */

void render_layout(MathNode *node);
void render_draw(const MathNode *node, int x, int y);

/* Force every node in a subtree to use the given FontTier.  Useful when
 * a layout overflows the available width: call this and re-run
 * render_layout() to obtain a smaller bounding box.  Note that this
 * overrides the auto-demotion that render_layout() does for scripts and
 * bigop limits, so callers should normally only demote to FONT_SMALL
 * (i.e. demote a NORMAL tree to SMALL when it overflows). */
void render_force_tier(MathNode *node, FontTier tier);

/* Debug utility: return human-readable name for a node type */
const char *render_type_name(MathNodeType type);

#endif /* RENDER_H */
