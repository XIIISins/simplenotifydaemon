#include <stdlib.h>

#include "dbus.h"
#include "list.h"
#include "init.h"
#include "format.h"

void get_args(int argc, char** argv);
int main (int argc, char** argv) {
    get_args(argc, argv);

    // Set up dbus connection
    if (!setup_debus()) {
        return EXIT_FAILURE;
    }

    // Wait for messages
    int success = EXIT_SUCCESS;
    DBusMessage* msg;
    while (1) {
        // Check list for needed updates
        list_walk();

        // Get messages
        dbus_connection_read_write(g_conn, 0); //Non-blocking
        msg = dbus_connection_pop_message(g_conn); //Fetch messages

        if (!msg) {
            // If we haven't gotten any messages, sleep.
            nanosleep(&g_update_interval, NULL);
            continue;
        }

        if (!handle_message(msg)) {
            // Otherwise, handle them
            success = EXIT_FAILURE;
            break;
        }
    }

    // Free alloced memory
    list_destroy();
    format_clean();

    // Free list
    return success;
}
