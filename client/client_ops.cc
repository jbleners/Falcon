#include "client_prot.h"
#include "FalconClient.h"

#include "common.h"

bool_t
client_null_1_svc(void* argp, void* result, struct svc_req *rqstp) {
    return 1;
}

bool_t
client_up_1_svc(client_up_arg* argp, void* result, struct svc_req *rqstp) {
    FalconClient* cl = FalconClient::GetInstance();
    cl->ReceivedUp(argp->client_tag, LayerId(argp->handle),
                   Generation(argp->generation.generation_len,
                              argp->generation.generation_val));
    return 1;
}

bool_t
client_down_1_svc(client_down_arg* argp, void* result, struct svc_req *rqstp) {
    FalconClient* cl = FalconClient::GetInstance();
    uint32_t falcon_status;
    if (!argp->killed && !argp->would_kill) {
        falcon_status = DOWN_FROM_REMOTE;
    } else if (argp->killed) {
        falcon_status = KILLED_BY_REMOTE;
    } else if (argp->would_kill) {
        falcon_status = REMOTE_WOULD_KILL;
    }
    cl->ReceivedDown(argp->client_tag, LayerId(argp->handle),
                     Generation(argp->generation.generation_len,
                                argp->generation.generation_val),
                     falcon_status, argp->layer_status);
    return 1;
}

int
client_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result,
                         caddr_t result) {
    return 1;
}
