#ifndef _NTFA_ENFORCER_ENFORCER_H_
#define _NTFA_ENFORCER_ENFORCER_H_

#include <async.h>
#include <arpc.h>
#include "spy_prot.h"
#include "client_prot.h"

#include <rpc/xdr.h>

#include <list>
#include <map>
#include <set>

// This structure holds the enforcer's state about clients
// id - client identifier
// clnt_ - the rpc client for sending callbacks to this client
// up_intervals_ - how often to notify the client of signs of life
//
struct FalconClient {
    str                             id;
    ptr<aclnt>                      clnt_;
    std::map<str, int32_t>          up_intervals_;
    uint32_t                        client_tag_;

    bool operator<(const FalconClient &other) const {
        return id < other.id;
    }
};

// Common enforcer code. Layer-specific enforcers inherit from this class and
// must implement the virtual routines listed
class Enforcer : public virtual refcount {
    public:
        Enforcer();
        virtual ~Enforcer();

        // Performs layer-specific initialization
        virtual void Init() = 0;

        // Runs the server. Does not return.
        void Run();

        // Daemonize the process
        void Daemonize(const char* logfile);

        // Make the process badass: (mlock, high priority)
        void Prioritize();

        // Start/Stop monitoring a target
        virtual void StartMonitoring(const ref<const str> target) = 0;
        virtual void StopMonitoring(const ref<const str> target) = 0;

        // Test whether a target can be monitored
        virtual bool InvalidTarget(const ref<const str> target) = 0;

        // Layer-specific action to kill a layer
        virtual void Kill(const ref<const str> target) = 0;

        // Layer-specific action to check generations. Should be a no-op
        // unless there is a layer-specific reason to track generations
        // outside of the enforcer.
        virtual void UpdateGenerations(const ref<const str> target) = 0;

    protected:

        // Received an up event from layer-specific code
        void ObserveUp(const ref<const str> target);

        // Received a down event from layer-specific code
        void ObserveDown(const ref<const str> target, const uint32_t status,
                         const bool killed, const bool would_kill);

        // Informs layer-specific code about use of lethal force
        bool Killable(const ref<const str> target) {
            return !deadly_[*target].empty();
        }

        // Used when layer-specific code is responsible for keeping track of
        // genrations
        void FastForwardGeneration(const ref<const str> target,
                                   uint32_t new_gen);

        // Get the generation for a specific target
        uint32_t GetGeneration(const ref<const str> target) {
            return generations_[*target];
        }

        // Directs LOG messages when in daemon mode
        const char* logfile_name_;

        // base generation vector
        rpc_vec<gen_no, RPC_INFINITY>                   gen_vec_;

    private:
        // List of target generations
        std::map<str, gen_no>                           generations_;

        // Will add a new client to all clients or get the existing client
        ref<FalconClient> GetClient(const client_addr_t& addr);

        // Remove a client. Will stop monitoring any targets it is the sole
        // client of.
        void RemoveClient(const ref<FalconClient> client);

        // Utility for checking generation
        spy_status GenCheck(const ref <const str> target,
                            const rpc_vec<gen_no, RPC_INFINITY>& gen_vec);

        // Handle a heartbeat from a client. Used for garbage collection of
        // crashed clients.
        void HandleClientHeartbeat(const ref<FalconClient> client,
                                   int retry_count, clnt_stat rpc_status);

        // Send heartbeat message to the client
        void DoHeartbeat(const ref<FalconClient> client, int retry_count);

        // Add client to list of clients waiting for an up response from this
        // layer
        void AddToWaiting(const ref<const str> target,
                          const ref<FalconClient> client);

        // Set timer for when this client will begin actively waiting for
        // signs of life.
        void RepeatWaiting(const ref<const str> target,
                           const ref<FalconClient> client);

        // Handle enforcer RPCs
        void Dispatch(svccb *sbp);

        // Send Down rpc call to this client
        void ReportDown(const ref<FalconClient> client,
                        const ref<client_down_arg> rpc_arg,
                        int32_t retries, clnt_stat rpc_status);

        // Initialize the generations at this layer
        void InitGenerations();

        // Increment the generation of handle by 1 and sync the local logs
        void IncrementGeneration(const ref<const str> target);

        // FILE* for the generation write-ahead-log
        FILE* generation_log_;

        // Set of all clients
        std::set<ref<FalconClient> > all_clients_;

        // Maps targets to clients waiting for signs of life from those targets
        std::map<str, std::set<ref<FalconClient> > > waiting_for_up_;

        // List of all clients for each target
        std::map<str, std::set<ref<FalconClient> > >    target_clients_;

        // rpc srv
        ptr<asrv>                                       srv_;

        // List of clients granting a license to kill.
        std::map<str, std::set<ref<FalconClient> > >    deadly_;
};
#endif  // _NTFA_ENFORCER_ENFORCER_H_
