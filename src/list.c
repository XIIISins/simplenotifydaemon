#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "list.h"
#include "init.h"
#include "format.h"
#include "dbus.h"

struct ListHead g_text = { .head = NULL, .changed = false };

/*Get current time to check against expiration*/
unsigned long current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static bool list_make_line_string(char** dest, const char* app, const char* sum, const char* body, dbus_int32_t nid) {
    /*New string. A little bit of padding, to be safe*/
    size_t string_size = g_format_container->min_size
    + ((g_format_container->has & FORM_LINE_APP)    ? (sizeof(char) * strlen(app))  : 0)
    + ((g_format_container->has & FORM_LINE_SUM)    ? (sizeof(char) * strlen(sum))  : 0)
    + ((g_format_container->has & FORM_LINE_BOD)    ? (sizeof(char) * strlen(body)) : 0)
    + ((g_format_container->has & FORM_LINE_ID)     ? (sizeof(char) * 20)           : 0)
    + ((g_format_container->has & FORM_LINE_IF_APP)
        ? ((*app == 0)
            ? (sizeof(char) * strlen(g_dformat.no_app))
            : (sizeof(char) * strlen(g_dformat.app)))
        : 0)
    + ((g_format_container->has & FORM_LINE_IF_BOD)
        ? ((*app == 0)
            ? (sizeof(char) * strlen(g_dformat.no_body))
            : (sizeof(char) * strlen(g_dformat.body)))
        : 0);
    *dest = malloc(string_size);
    if (!(*dest)) return false;

    sprintf(*dest, "\0");
    for (int x = 0; x < g_format_container->len; x++) {
        if (g_format_container->array[x]->is_seperator) {
            sprintf(*dest + strlen(*dest), "%s",
            g_format_container->array[x]->content.seperator);
        } else {
            switch (g_format_container->array[x]->content.specifier) {
                case FORM_LINE_APP:
                    sprintf(*dest + strlen(*dest), "%s", app);
                    break;
                case FORM_LINE_SUM:
                    sprintf(*dest + strlen(*dest), "%s", sum);
                    break;
                case FORM_LINE_BOD:
                    sprintf(*dest + strlen(*dest), "%s", body);
                    break;
                case FORM_LINE_ID:
                    sprintf(*dest + strlen(*dest), "%lu", nid);
                    break;
                case FORM_LINE_IF_APP:
                    sprintf(*dest + strlen(*dest),  "%s", (*app == 0) ? g_dformat.no_app : g_dformat.app);
                    break;
                case FORM_LINE_IF_BOD:
                    sprintf(*dest + strlen(*dest),  "%s", (*body == 0) ? g_dformat.no_body : g_dformat.body);
                    break;
            }
        }
    }
    return true;
}

static bool list_make_status_string(char** dest, int count, bool new, bool body) {
    /*New string. A little bit of padding, to be safe*/
    size_t string_size = g_status_container->min_size
    + ((g_status_container->has & FORM_STAT_NEW)
        ? ((new)
            ? (sizeof(char) * strlen(g_dformat.new))
            : (sizeof(char) * strlen(g_dformat.no_new)))
        : 0)
    + ((g_status_container->has & FORM_STAT_PEND)
        ? ((body)
            ? (sizeof(char) * strlen(g_dformat.pending))
            : (sizeof(char) * strlen(g_dformat.no_pending)))
        : 0);
    *dest = malloc(string_size);
    if (!(*dest)) return false;

    sprintf(*dest, "\0");
    for (int x = 0; x < g_status_container->len; x++) {
        if (g_status_container->array[x]->is_seperator) {
            sprintf(*dest + strlen(*dest), "%s",
            g_status_container->array[x]->content.seperator);
        } else {
            switch (g_status_container->array[x]->content.specifier) {
                case FORM_STAT_NUM:
                    sprintf(*dest + strlen(*dest), "%d", count);
                    break;
                case FORM_STAT_NEW:
                    sprintf(*dest + strlen(*dest),  "%s", (new) ? g_dformat.new : g_dformat.no_new);
                    break;
                case FORM_STAT_PEND:
                    sprintf(*dest + strlen(*dest),  "%s", (body) ? g_dformat.pending : g_dformat.no_pending);
                    break;
            }
        }
    }
    return true;
}

/*Allocate new node*/
static NotifyLine* list_new(const char* app, const char* sum, const char* body, dbus_int32_t expires, dbus_uint32_t nid) {
    /*New node*/
    NotifyLine* line = malloc(sizeof(NotifyLine));
    if (!line) return NULL;

    /*Make line*/
    if (!list_make_line_string(&line->line, app, sum, body, nid)) {
        return NULL;
    }

    if (expires > -1)  line->expires = (unsigned long)expires + current_time();
    else line->expires = current_time() + g_sleep_time;
    line->nid = nid;
    line->next = NULL;
    line->deleted = false;

    return line;
}

/*Appends a node*/
static void list_add(NotifyLine* line) {
    if (!g_text.head) {
        g_text.head = line;
        g_text.changed = true;
        return;
    }

    NotifyLine* current_line = g_text.head;
    while (current_line->next) {
        current_line = current_line->next;
    }

    current_line->next = line;
    return;
}

/*Updates a node*/
bool list_update(const char* app, const char* sum, const char* body, dbus_uint32_t nid, bool to_delete) {
    bool found = false;
    NotifyLine* current_line = g_text.head;
    while (current_line) {
        if (current_line->nid == nid) {
            found = true;
            break;
        }
        current_line = current_line->next;
    }
    if (found) {
        if (!to_delete) {
            free(current_line->line);
            list_make_line_string(&current_line->line, app, sum, body, nid);
            g_text.changed = true; // Force update
        } else {
            current_line->expires = current_time();
            current_line->deleted = false;
            g_text.changed = true; // Force update
        }
        return true;
    }
    return false;
}

/*Makes a new node and appends it*/
bool list_append(const char* app, const char* sum, const char* body, dbus_int32_t expires, dbus_uint32_t nid) {
    NotifyLine* line = list_new(app, sum, body, expires, nid);
    if (!line) {
        return false;
    }
    list_add(line);
    g_text.changed = true;
    g_text.new = true;
    return true;
}

/*Frees a node*/
static void list_free(NotifyLine* line) {
    free(line->line);
    free(line);
}

/*Unlinks a node*/
static void list_remove(NotifyLine* line) {
    if (!line) {
        return;
    }
    if (line == g_text.head) {
        g_text.head = g_text.head->next;
    } else {
        NotifyLine* current_line = g_text.head;
        while (current_line->next != line) {
            current_line = current_line->next;
            if (!current_line) return;
        }
        current_line->next = current_line->next->next;
    }
    list_free(line);
    g_text.changed = true;
}

/*Free whole list*/
void list_destroy() {
    while (g_text.head) {
        list_remove(g_text.head);
    }
}

static int list_count() {
    int ret = 0;
    NotifyLine* needle = g_text.head;
    while (needle) {
        needle = needle->next;
        ret++;
    }
    return ret;
}

/*Checks for zombies/removes them/prints if new/deleted lines*/
bool list_walk() {
    unsigned long time = current_time();
    NotifyLine* current_line = g_text.head;
    NotifyLine* to_chopping_block = NULL;

    /*Find zombies*/
    while (current_line) {
        if (current_line->expires < time) {
            signal_notificationclose(current_line->nid,
                (current_line->deleted) ? 3 : 1);
            to_chopping_block = current_line;
            current_line = current_line->next;
            list_remove(to_chopping_block);
            continue;
        }
        current_line = current_line->next;
    }
    if (!g_text.changed) return true;

    /*Print stuff!*/
    /*Adjust $LINES*/
    int list_len = list_count();
    int for_inc = 1;
    if (!g_dzen) fprintf(stdout, " \n");
    if ((g_status_top) && (*g_default_status)) {
        if (!g_dzen) {
            for_inc += 1;
        }
        char* statusline = NULL;
        if (!list_make_status_string(&statusline,
            (g_lines > 0) ? ((((list_len - g_lines) > 0) ? g_lines : list_len) - for_inc + 2) : list_len,
            g_text.new, (g_text.head))) return false;
        fprintf(stdout, "%s\n", statusline);
        free(statusline);
    }
    current_line = g_text.head;
    if (g_lines > 0) {
        /*If using $LINES*/
        int to_skip = list_len - g_lines + for_inc;
        for (int x = 0; x < to_skip; x++) {
            current_line = current_line->next;
        }
        for (int x = 0; x < g_lines - for_inc; x++) {
            if (current_line) {
                /*Print queue*/
                fprintf(stdout, "%s\n", current_line->line);
                current_line = current_line->next;
            } else {
                /*Print blank lines*/
                fprintf(stdout, " \n");
            }
        }
    } else {
        /*Else just print the group*/
        while (current_line) {
            fprintf(stdout, "%s\n", current_line->line);
            current_line = current_line->next;
        }
    }
    if ((!g_status_top) && (*g_default_status)) {
        char* statusline = NULL;
        if (!list_make_status_string(&statusline,
            (g_lines > 0) ? ((((list_len - g_lines) > 0) ? g_lines : list_len) - for_inc + 1) : list_len,
            g_text.new, (g_text.head))) return false;
        fprintf(stdout, "%s%s", statusline, (g_dzen) ? "\n" : "");
        free(statusline);
    }
    fflush(stdout);
    /*Reset vars*/
    g_text.changed = false;
    g_text.new = false;
    return true;
}
