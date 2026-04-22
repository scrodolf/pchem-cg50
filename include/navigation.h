/* ==========================================================================
 * navigation.h - Global keyword/symbol navigator
 * ==========================================================================
 *
 * PURPOSE
 * -------
 * Displays ALL keywords and symbols from every topic, organized by topic.
 * Each topic is a section with its title as a heading, followed by its
 * keywords and the OS-font symbols used within it.
 *
 * The screen is fully scrollable (UP/DOWN by line, F1/F2 by page).
 *
 * DESIGN
 * ------
 * NavigationScreen is simply a scroll offset + the lazily-computed total
 * content height.  Content comes directly from the per-topic KeywordEntry
 * arrays declared in topics.c (accessed via topic_content()).
 * ========================================================================== */

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "pchem.h"

typedef struct {
    int scroll_y;        /* Current scroll offset in pixels                 */
    int content_h;       /* Total rendered content height (set on draw)     */
} NavigationScreen;

void navigation_init(NavigationScreen *ns);
void navigation_draw(NavigationScreen *ns);   /* mutates content_h          */

/* Returns:
 *   0 = key consumed (scroll)
 *   1 = EXIT (go back to main menu)
 *   2 = MENU (exit the add-in) */
int  navigation_handle_key(NavigationScreen *ns, key_event_t ev);

#endif /* NAVIGATION_H */
