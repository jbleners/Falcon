#include "FalconClient.h"
#include "common.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

// This is declared to link with the svc_program we implement with client_ops
void client_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

namespace {
// Singleton business
pthread_once_t once_ctl_ = PTHREAD_ONCE_INIT;
FalconClient* client_ = NULL;
SVCXPRT* srv_trans_ = NULL;
pthread_mutex_t svc_start_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t svc_start_cond_ = PTHREAD_COND_INITIALIZER;

// Request queue!
enum req_type {STOP, START};

}  // end anonymous namespace

class FalconRequest {
public:
    explicit FalconRequest(const LayerIdList& h) :
        type(STOP),
        idlist(h),
        client_data(NULL),
        lethal(false),
        cb(NULL),
        e2etimeout(-1),
        done(false),
        target(NULL) {
            pthread_mutex_init(&lock, NULL);
            pthread_cond_init(&cond, NULL);
        }

    FalconRequest(const LayerIdList& h, void* cd, bool l,
                  falcon_callback_fn cb, int32_t to) :
        type(START),
        idlist(h),
        client_data(cd),
        lethal(l),
        cb(cb),
        e2etimeout(to),
        done(false),
        target(NULL) {
            pthread_mutex_init(&lock, NULL);
            pthread_cond_init(&cond, NULL);
        }

    ~FalconRequest() {
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&lock);
    }

    falcon_target* GetCompleted() {
        falcon_target* ret = NULL;
        pthread_mutex_lock(&lock);
        while (!done) {
            pthread_cond_wait(&cond, &lock);
        }
        ret = target;
        pthread_mutex_unlock(&lock);
        return ret;
    }

    void Complete(falcon_target* t) {
        pthread_mutex_lock(&lock);
        target = t;
        done = true;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);
    }
    const req_type              type;
    const LayerIdList           idlist;
    void*                       client_data;
    const bool                  lethal;
    const falcon_callback_fn    cb;
    const int32_t               e2etimeout;

    bool                        done;
    falcon_target*              target;
    pthread_mutex_t             lock;
    pthread_cond_t              cond;
};

const uint32_t kHostnameSize = 128;
void*
start_svc(void*) {
    pthread_mutex_lock(&svc_start_lock_);
    // Get the socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(fd >= 0);

    // Get the external IP
    char namebuf[kHostnameSize];
    CHECK(0 == gethostname(namebuf, kHostnameSize));
    addrinfo hints;
    addrinfo* aret = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    CHECK(0 == getaddrinfo(namebuf, NULL, &hints, &aret));

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    // Create the transport
    CHECK(0 == bind(fd, aret->ai_addr, len));
    srv_trans_ = svcudp_create(fd);
    CHECK(srv_trans_);
    CHECK(1 == svc_register(srv_trans_, CLIENT_PROG, CLIENT_V1, client_prog_1,
                            0));

    // Signal that this thread is done
    pthread_cond_signal(&svc_start_cond_);
    pthread_mutex_unlock(&svc_start_lock_);

    // Run it!
    svc_run();  // Never returns
    CHECK(false);
    return NULL;
}

void*
start_builder(void*) {
    // Builder will wait here until the constructor is finished.
    FalconClient* cl = FalconClient::GetInstance();
    pthread_mutex_t* q_lock = &cl->queue_lock_;
    pthread_cond_t*  q_cond = &cl->queue_cond_;
    for (;;) {
        pthread_mutex_lock(q_lock);
        while (cl->request_q_.empty()) {
            pthread_cond_wait(q_cond, q_lock);
        }
        ReqPtr req = cl->request_q_.front();
        client_addr_t addr = cl->addr_;
        cl->request_q_.pop();
        pthread_mutex_unlock(q_lock);
        if (req->type == STOP) {
            // TODO(leners):
            // Find the where to cut the tree, and cut it. The watchdog will
            // take care of cancellation.
            continue;
        } else {
            FalconLayerPtr curr_layer = cl->base_layer_;
            FalconCallbackPtr cb = FalconCallbackPtr(
                                        new FalconCallback(req->cb,
                                                           req->idlist,
                                                           req->client_data));
            // i == 0 when we are dealing with the App itself, in this case
            // there is no need for a LayerId to be put in the catalog
            ssize_t i;
            for (i = req->idlist.size()-1; i >= 0 && curr_layer; --i) {
                addr.client_tag = cl->GetNextLayerId();
                curr_layer = curr_layer->AddChild(req->idlist[i], req->lethal,
                                                  addr, req->e2etimeout,
                                                  (i == 0), curr_layer, cb);
                if (curr_layer && i != 0) {
                    cl->AddLayer(addr.client_tag, curr_layer);
                }
            }
            if (!curr_layer) {
                (*cb)(req->idlist[i], REGISTRATION_ERROR, 0);
            } else {
                req->Complete(new falcon_target(cb, curr_layer));
                LOG("successfully registered app!");
            }
        }
    }
    return NULL;
}

void
FalconClient::AddLayer(uint32_t cid, FalconLayerPtr layer) {
    pthread_mutex_lock(&layer_map_lock_);
    CHECK(!layer_map_[cid].lock());
    layer_map_[cid] = layer;
    pthread_mutex_unlock(&layer_map_lock_);
}

FalconLayerPtr
FalconClient::GetLayer(uint32_t cid) {
    FalconLayerPtr ret;
    pthread_mutex_lock(&layer_map_lock_);
    ret = layer_map_[cid].lock();
    pthread_mutex_unlock(&layer_map_lock_);
    return ret;
}

void
FalconClient::ReceivedUp(uint32_t cid, LayerId handle, Generation gen) {
    FalconLayerPtr layer = GetLayer(cid);
    if (!layer) {
        LOG("Up for non-present layer %s:%u", handle.c_str(), cid);
    } else {
      layer->DoChildUp(handle, gen);
    }
    return;
}

void
FalconClient::ReceivedDown(uint32_t cid, LayerId handle, Generation gen,
                           uint32_t falcon_status, uint32_t remote_status) {
    FalconLayerPtr layer = GetLayer(cid);
    if (!layer) {
        LOG("Down for non-present layer %s:%u", handle.c_str(), cid);
    } else {
      layer->DoChildDown(handle, gen, falcon_status, remote_status);
    }
    return;
}

uint32_t
FalconClient::GetNextLayerId() {
    uint32_t ret;
    pthread_mutex_lock(&layer_id_lock_);
    layer_id_counter_++;
    ret = layer_id_counter_;
    pthread_mutex_unlock(&layer_id_lock_);
    return ret;
}


FalconClient::FalconClient() {
    // 1. Initialize concurrency control
    pthread_mutex_init(&queue_lock_, NULL);
    pthread_cond_init(&queue_cond_, NULL);
    pthread_mutex_init(&layer_id_lock_, NULL);
    pthread_mutex_init(&layer_map_lock_, NULL);

    // 2. Create the svc thread
    pthread_t svc;
    pthread_create(&svc, NULL, start_svc, NULL);
    pthread_detach(svc);

    // 2b. Wait for the transport to be created. Construct the addr
    pthread_mutex_lock(&svc_start_lock_);
    while (srv_trans_ == NULL) {
        pthread_cond_wait(&svc_start_cond_, &svc_start_lock_);
    }
    pthread_mutex_unlock(&svc_start_lock_);
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    CHECK(0 == getsockname(srv_trans_->xp_sock,
                           reinterpret_cast<sockaddr *>(&addr), &len));
    addr_.ipaddr = addr.sin_addr.s_addr;
    addr_.port = addr.sin_port;
    addr_.client_tag = layer_id_counter_ = 0;

    // 3. Create management thread.
    pthread_t builder;
    pthread_create(&builder, NULL, start_builder, NULL);
    pthread_detach(builder);

    // 4. Initialize the base layer
    base_layer_ = FalconLayerPtr(
                    new FalconLayer(LayerId(),  // NULL handle
                                    Generation(0, NULL),  // NULL gen
                                    false,  // Not killable
                                    FalconParentPtr(),  // NULL Paernt
                                    addr_,  // first addr
                                    0,  // no timeout,
                                    false,  // Not a watchdog
                                    true,  // is base
                                    NULL));  // Can't connect
}

falcon_target*
FalconClient::StartMonitoring(const LayerIdList& handle, bool lethal,
                              falcon_callback_fn cb, void* cd,
                              int32_t e2etimeout) {
    pthread_mutex_lock(&queue_lock_);
    ReqPtr req = ReqPtr(new FalconRequest(handle, cd, lethal, cb, e2etimeout));
    request_q_.push(req);
    pthread_cond_signal(&queue_cond_);
    pthread_mutex_unlock(&queue_lock_);
    return req->GetCompleted();
}

void
FalconClient::StopMonitoring(const LayerIdList& handle) {
    pthread_mutex_lock(&queue_lock_);
    ReqPtr req = ReqPtr(new FalconRequest(handle));
    request_q_.push(req);
    pthread_cond_signal(&queue_cond_);
    pthread_mutex_unlock(&queue_lock_);
    return;
}

void
init_func() {
    client_ = new FalconClient;
}

FalconClient*
FalconClient::GetInstance() {
    // No errors are defined for this function.
    pthread_once(&once_ctl_, init_func);
    return client_;
}
