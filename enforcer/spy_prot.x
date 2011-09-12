#include "status.h"

typedef uint32_t status_t;
typedef uint32_t gen_no;

/* Describes a target at a spy.
   handle: the string identifier of a target
   generation: list of generation numbers below the current layer
*/
struct target_t {
    string handle<>;
    gen_no generation<>;
};

struct client_addr_t {
    uint32_t    ipaddr;
    uint32_t    port;
    uint32_t    client_tag;
};

/* Arguments to the register RPC */
struct spy_register_arg {
    target_t        target;
    bool            lethal;
    int32_t         up_interval_ms;
    client_addr_t   client;
};

struct spy_cancel_arg {
    target_t        target;
    client_addr_t   client;
};

struct spy_kill_arg {
    target_t    target;
};

struct spy_get_gen_arg {
    target_t    target;
};

struct spy_res {
    target_t    target;
    status_t    status;
};

program SPY_PROG {
    version SPY_V1 {
        void
        SPY_NULL(void) = 0;

        spy_res
        SPY_REGISTER(spy_register_arg) = 1;

        spy_res
        SPY_CANCEL(spy_cancel_arg) = 2;

        spy_res
        SPY_KILL(spy_kill_arg) = 3;

        spy_res
        SPY_GET_GEN(spy_get_gen_arg) = 4;
    } = 1;
} = 2000111;
