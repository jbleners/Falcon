#include "client.h"
#include "FalconClient.h"
#include "Watchdog.h"

namespace {
const char* status_msg[9] = {
    "down, not killed",
    "down, killed",
    "down, would kill",
    "remote error",
    "unknown error",
    "end-to-end timeout expired",
    "down, learned from end-to-end timeout",
    "registration error",
    "signs of life"
};
}

void
Falcon::LogCallbackData(const LayerIdList& id_list, uint32_t falcon_status,
                uint32_t remote_st) {
    int idx = (falcon_status >> 16);
    uint32_t fs = falcon_status ^ (idx << 16);
    LOG("layer: %s falcon: %s remote: %u", id_list[idx].c_str(),
        status_msg[fs], remote_st);
}

falcon_target*
Falcon::init(const LayerIdList& id_list, bool lethal, void* client_data,
     int up_callback_period) {
    FalconClient* cl = FalconClient::GetInstance();
    falcon_target* target = cl->StartMonitoring(id_list, lethal, NULL,
                                                client_data,
                                                up_callback_period);
    return target;
}

void
Falcon::uninit(falcon_target* target) {
    FalconLayerPtr t = target->top_layer.lock();
    if (t) t->Cancel();
}

void
Falcon::startTimer(falcon_target* target, int e2etimeout) {
    FalconLayerPtr t = target->top_layer.lock();
    if (t && e2etimeout > 0) t->StartTimer(target->cb, e2etimeout);
}

void
Falcon::setCallback(falcon_target* target, falcon_callback_fn cb) {
    target->cb->Reactivate(cb);
}

void
Falcon::startMonitoring(falcon_target* target, falcon_callback_fn cb, int e2etimeout) {
    setCallback(target, cb);
    startTimer(target, e2etimeout);
}

bool
Falcon::query_alive(falcon_target* target) {
  return !target->cb->HasRunFinal();
}

void
Falcon::stopTimer(falcon_target* target) {
    FalconLayerPtr t = target->top_layer.lock();
    if (t) t->StopTimer();
}

void
Falcon::removeCallback(falcon_target* target) {
    target->cb->Deactivate();
}

void
Falcon::stopMonitoring(falcon_target* target) {
  stopTimer(target);
  removeCallback(target);
}
