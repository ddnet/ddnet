#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define DDNET_NET_EV_NONE 0

#define DDNET_NET_EV_CONNECT 1

#define DDNET_NET_EV_CHUNK 2

#define DDNET_NET_EV_DISCONNECT 3

typedef struct DdnetNet DdnetNet;


typedef union DdnetNetEvent {
  uint64_t _padding_alignment[8];
} DdnetNetEvent;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

uint64_t ddnet_net_ev_kind(const union DdnetNetEvent *ev);

const uint8_t (*ddnet_net_ev_connect_identity(const union DdnetNetEvent *ev))[32];

size_t ddnet_net_ev_chunk_len(const union DdnetNetEvent *ev);

bool ddnet_net_ev_chunk_is_unreliable(const union DdnetNetEvent *ev);

size_t ddnet_net_ev_disconnect_reason_len(const union DdnetNetEvent *ev);

bool ddnet_net_ev_disconnect_is_remote(const union DdnetNetEvent *ev);

bool ddnet_net_new(struct DdnetNet **net);

bool ddnet_net_set_bindaddr(struct DdnetNet *net, const char *addr, size_t addr_len);

bool ddnet_net_set_identity(struct DdnetNet *net, const uint8_t (*private_identity)[32]);

bool ddnet_net_set_accept_incoming_connections(struct DdnetNet *net, bool accept);

bool ddnet_net_open(struct DdnetNet *net);

void ddnet_net_free(struct DdnetNet *net);

const char *ddnet_net_error(const struct DdnetNet *net);

size_t ddnet_net_error_len(const struct DdnetNet *net);

bool ddnet_net_wait(struct DdnetNet *net);

bool ddnet_net_wait_timeout(struct DdnetNet *net, uint64_t ns);

bool ddnet_net_recv(struct DdnetNet *net,
                    uint8_t *buf,
                    size_t buf_cap,
                    uint64_t *peer_index,
                    union DdnetNetEvent *event);

bool ddnet_net_send_chunk(struct DdnetNet *net,
                          uint64_t peer_index,
                          const uint8_t *chunk,
                          size_t chunk_len,
                          bool unreliable);

bool ddnet_net_connect(struct DdnetNet *net,
                       const char *addr,
                       size_t addr_len,
                       uint64_t *peer_index);

bool ddnet_net_close(struct DdnetNet *net,
                     uint64_t peer_index,
                     const char *reason,
                     size_t reason_len);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
