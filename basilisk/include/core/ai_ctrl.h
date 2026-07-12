#ifndef CORE_AI_CTRL_H
#define CORE_AI_CTRL_H

#include <glib.h>

// Callback signature for receiving AI response chunks.
// chunk is NULL when the stream is finished or failed.
typedef void (*AiCallback)(const gchar *chunk, gboolean is_done, gpointer user_data);

// Start an AI request asynchronously
void ai_ctrl_fetch_response(const gchar *query, AiCallback callback, gpointer user_data);

// Cleanup pending requests
void ai_ctrl_cleanup(void);

#endif
