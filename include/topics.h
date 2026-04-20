/* ==========================================================================
 * topics.h — Topic screen content and dispatch
 * ==========================================================================
 * Each PChem topic gets its own screen handler.  For now these are stubs
 * that display the topic title and a placeholder.  Later, each will build
 * MathNode trees and render equations alongside explanatory text.
 * ========================================================================== */

#ifndef TOPICS_H
#define TOPICS_H

#include "pchem.h"

/* -----------------------------------------------------------------------
 * TopicScreen — runtime state for a topic content screen
 * ----------------------------------------------------------------------- */
typedef struct {
    TopicID     id;             /* Which topic is active                    */
    int         scroll_y;       /* Vertical scroll offset in pixels        */
    int         content_h;      /* Total content height (for scroll limit) */
} TopicScreen;

/* Initialize a topic screen for the given topic */
void topic_init(TopicScreen *ts, TopicID id);

/* Draw the topic screen to VRAM (does NOT call dupdate) */
void topic_draw(const TopicScreen *ts);

/* Handle a key event.  Returns 1 if the caller should go back (EXIT
 * was pressed), 2 if the app should quit (MENU), 0 otherwise. */
int  topic_handle_key(TopicScreen *ts, key_event_t ev);

/* Get the human-readable title for a topic ID */
const char *topic_title(TopicID id);

/* Get a short description string for a topic ID */
const char *topic_description(TopicID id);

#endif /* TOPICS_H */
