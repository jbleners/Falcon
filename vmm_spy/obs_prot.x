// Both the enforcer and observer run a server/client running this protocol
// The Enforcer ignores probes, while the observer ignores register/cancel
struct obs_register_arg {
    string handle<>;
};

struct obs_cancel_arg {
    string handle<>;
};

struct obs_register_response {
    uint32_t generation;
};

struct obs_probe_msg {
    uint32_t counter;
};

program VMM_OBS_PROG {
    version VMM_OBS_V1 {
        void
        VMM_OBS_NULL(void) = 0;

        obs_register_response
        VMM_OBS_REGISTER(obs_register_arg) = 1;

        void
        VMM_OBS_CANCEL(obs_cancel_arg) = 2;

        obs_probe_msg
        VMM_OBS_PROBE(obs_probe_msg) = 3;
    } = 1;
} = 2000500;
