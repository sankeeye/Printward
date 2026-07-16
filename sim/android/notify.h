#pragma once
#ifndef NOTIFY_H
#define NOTIFY_H

// Push notifications via ntfy.sh (https). Set a topic in printward.conf
// (ntfy_topic=) or via the web Settings; leave empty to disable. Fires when a
// print finishes/fails and when the active spool will run short.

extern char g_ntfy_topic[64];

void notify_loop();                                   // detect events, fire
void notify_send(const char* title, const char* msg); // fire-and-forget (bg thread)

#endif
