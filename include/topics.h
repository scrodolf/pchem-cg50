/* ==========================================================================
 * topics.h - Topic screens, submenus, and content data model
 * ==========================================================================
 *
 * OVERVIEW
 * --------
 * A "topic" (Particle in a Box, Spectroscopy, Hydrogen Atom, ...) is
 * divided into three "subtopics":
 *
 *   DESCRIPTIONS  -- paragraph-style overview of the topic
 *   EQUATIONS     -- each equation labelled, with variable descriptions
 *   KEYWORDS      -- list of key terms with short definitions
 *
 * Navigation flow:
 *   Main menu  -->  Per-topic submenu  -->  Content view for one subtopic.
 *
 * CONTENT DATA MODEL
 * ------------------
 * All content is declared as static const arrays inside topics.c.
 * For each of the 6 topics we store:
 *   - A TopicContent struct containing: title, description paragraphs,
 *     equations (with label + variable-description + MathBuilder), and
 *     keywords (with name + short definition).
 *
 * EquationEntry uses a function pointer (MathBuilder) instead of a
 * pre-built MathNode* because the node pool is reset per screen - each
 * visit rebuilds the AST on demand.  This matches the existing
 * sample_builders[] pattern in the original topics.c.
 * ========================================================================== */

#ifndef TOPICS_H
#define TOPICS_H

#include "pchem.h"
#include "render.h"

/* -----------------------------------------------------------------------
 * Equation builder function pointer.
 * Called each time an equation is rendered to (re)build its MathNode AST.
 * ----------------------------------------------------------------------- */
typedef MathNode *(*MathBuilder)(void);

/* -----------------------------------------------------------------------
 * EquationEntry - one equation plus its human-readable metadata
 *
 * Example:
 *   { "Reduced Mass",
 *     build_reduced_mass,
 *     "mu (reduced mass)\n"
 *     "m1, m2 (individual body masses)" }
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *label;          /* Short title, e.g. "Reduced Mass"       */
    MathBuilder builder;         /* Builds the MathNode AST                */
    const char *vars;            /* Variable-description block (multi-line)*/
} EquationEntry;

/* -----------------------------------------------------------------------
 * KeywordEntry - one keyword with a short definition
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *name;            /* Term, e.g. "Reduced Mass (mu)"         */
    const char *definition;      /* One-to-two-sentence definition         */
} KeywordEntry;

/* -----------------------------------------------------------------------
 * TopicContent - aggregate content for one topic
 *
 * description_paragraphs is a single multi-line, "\n\n"-delimited string.
 * Drawing code uses simple word-wrapping on each paragraph.
 * ----------------------------------------------------------------------- */
typedef struct {
    const char           *title;             /* Display title              */
    const char           *description_block; /* Full description paragraphs */
    const EquationEntry  *equations;         /* Array                      */
    int                   num_equations;
    const KeywordEntry   *keywords;          /* Array                      */
    int                   num_keywords;
} TopicContent;

/* -----------------------------------------------------------------------
 * Runtime state structs
 * ----------------------------------------------------------------------- */

/* Submenu screen: shows "1. Descriptions / 2. Equations / 3. Keywords"
 * for the currently selected topic. */
typedef struct {
    TopicID  topic;             /* Which topic this submenu is for         */
    int      sel;               /* Selected subtopic index                 */
} SubMenuScreen;

/* Topic content screen: scrollable display of one subtopic's content. */
typedef struct {
    TopicID     topic;          /* Parent topic                            */
    SubtopicID  subtopic;       /* Which subsection is shown               */
    int         scroll_y;       /* Current scroll offset in pixels         */
    int         content_h;      /* Total rendered content height           */
} TopicScreen;

/* -----------------------------------------------------------------------
 * Public lookup helpers
 * ----------------------------------------------------------------------- */
const char          *topic_title(TopicID id);
const TopicContent  *topic_content(TopicID id);  /* NULL if invalid        */

/* -----------------------------------------------------------------------
 * Submenu API
 * ----------------------------------------------------------------------- */
void submenu_init(SubMenuScreen *sm, TopicID topic);
void submenu_draw(const SubMenuScreen *sm);

/* Returns:
 *   0 = key consumed (scroll/nav)
 *   1 = user selected a subtopic (sm->sel holds SubtopicID)
 *   2 = EXIT  (go back to main menu)
 *   3 = MENU  (exit the add-in) */
int  submenu_handle_key(SubMenuScreen *sm, key_event_t ev);

/* -----------------------------------------------------------------------
 * Topic content screen API
 * ----------------------------------------------------------------------- */
void topic_init(TopicScreen *ts, TopicID topic, SubtopicID subtopic);
void topic_draw(TopicScreen *ts);   /* mutates ts->content_h on first draw */

/* Returns:
 *   0 = key consumed (scroll)
 *   1 = EXIT (go back to submenu)
 *   2 = MENU (exit the add-in) */
int  topic_handle_key(TopicScreen *ts, key_event_t ev);

#endif /* TOPICS_H */
