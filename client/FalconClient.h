#ifndef _NTFA_FALCON_CLIENT_FALCONCLIENT_H_
#define _NTFA_FALCON_CLIENT_FALCONCLIENT_H_

#include <boost/smart_ptr.hpp>
#include <pthread.h>
#include <queue>

#include "Generation.h"
#include "FalconLayer.h"

#include "client_prot.h"
#include "client_status.h"

const int32_t kDefaultFalconInterval = 60;  // One minute
struct FalconRequest;
typedef boost::shared_ptr<FalconRequest> ReqPtr;

// Prints Falcon status in a human readable format.
void LogCallbackData(const LayerIdList& id_list, uint32_t falcon_status,
                     uint32_t remote_st);

struct falcon_target {
    falcon_target(FalconCallbackPtr cb_, FalconParentPtr top_layer_) :
        cb(cb_), top_layer(top_layer_) {}
    FalconCallbackPtr cb;
    FalconParentPtr   top_layer;
};

// This class is a singleton representing the Falcon coordinator. All methods
// are threadsafe.
class FalconClient {
    public:
        // Get an instance of the client. Will initialize a new instance if
        // no current instance exists.
        static FalconClient* GetInstance();

        // Start monitoring
        falcon_target* StartMonitoring(const LayerIdList& handle, bool lethal,
                                       falcon_callback_fn cb, void* client_data,
                                       int32_t up_interval = kDefaultFalconInterval);
        // Stop Monitoring
        void StopMonitoring(const LayerIdList& handle);

        void Kill(const LayerIdList& lids);

        // The RPC methods of the callback server
        friend bool_t client_up_1_svc(client_up_arg*, void*, svc_req*);
        friend bool_t client_down_1_svc(client_down_arg*, void*, svc_req*);
        friend bool_t client_null_1_svc(void*, void*, svc_req*);

        // Used to create the two falcon utility threads
        friend void*  start_svc(void*);
        friend void*  start_builder(void*);

        // Used to initialize a new FalconClient.
        friend void   init_func();

    protected:
        // Should only be called by init_func() above
        FalconClient();

        // Passed by the RPC server to the FalconClient object.
        void            ReceivedUp(uint32_t cid, LayerId handle,
                                   Generation gen);
        void            ReceivedDown(uint32_t cid, LayerId handle,
                                     Generation gen, uint32_t fs, uint32_t rs);

        // GetNextLayerId returns a client id (cid) to label a new layer
        uint32_t        GetNextLayerId();

        // Add a layer to the client's map of layers.
        void            AddLayer(uint32_t cid, FalconLayerPtr layer);

        // Get a layer from the client's map of layers.
        FalconLayerPtr  GetLayer(uint32_t cid);

        // Internal request queue synchronization and state.
        pthread_mutex_t                     queue_lock_;
        pthread_cond_t                      queue_cond_;
        std::queue<ReqPtr>                  request_q_;

        // client addr for tagging at the spies.
        client_addr_t                       addr_;

    private:
        // Layer management
        FalconLayerPtr                      base_layer_;
        std::map<uint32_t, FalconParentPtr> layer_map_;  // weak ptr
        pthread_mutex_t                     layer_map_lock_;

        // For atomic counting of layer id's
        pthread_mutex_t                     layer_id_lock_;
        uint32_t                            layer_id_counter_;

        // No deleting
        ~FalconClient();
};
#endif  // _NTFA_FALCON_CLIENT_FALCONCLIENT_H_
