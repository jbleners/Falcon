/* These are the asynchronous replies to the client. They are either a periodic
 * up message or a down message. handle identifies the target, generation is
 * present for sanity checking, and layer_status is a layer-defined status
 * message.
 */

struct client_up_arg {
    string      handle<>;
    uint32_t    generation<>;
    uint32_t    client_tag;
};

struct client_down_arg {
    string      handle<>;
    uint32_t    generation<>;
    uint32_t    layer_status;
    bool        killed;
    bool        would_kill;
    uint32_t    client_tag;
};

program CLIENT_PROG {
    version CLIENT_V1 {
        void
        CLIENT_NULL(void) = 0;

        void
        CLIENT_UP(client_up_arg) = 1;

        void
        CLIENT_DOWN(client_down_arg) = 2;
    } = 1;
} = 2000112;
