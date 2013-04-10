#ifndef __DIVA_CAPI_STREAMING_UTILS_H__
#define __DIVA_CAPI_STREAMING_UTILS_H__

extern int capi_DivaStreamingSupported(unsigned controller);
extern void capi_DivaStreamingOn(struct capi_pvt *i, unsigned char streamCommand, _cword messageNumber);
extern void capi_DivaStreamingStreamNotUsed(struct capi_pvt *i, byte streamCommand, _cword messageNumber);
extern void capi_DivaStreamingRemoveInfo(struct capi_pvt *i);
extern void capi_DivaStreamingRemove(struct capi_pvt *i);
extern void divaStreamingWakeup(void);
extern unsigned int capi_DivaStreamingGetStreamInUse(const struct capi_pvt* i);
extern void capi_DivaStreamLock(void);
extern void capi_DivaStreamUnLock (void);
extern void capi_DivaStreamingDisable (void);

typedef enum _diva_stream_state {
  DivaStreamCreated        = 0,
  DivaStreamActive         = 1,
  DivaStreamCancelSent     = 2,
  DivaStreamDisconnectSent = 3,
  DivaStreamDisconnected   = 4
} diva_stream_state_t;

typedef struct _diva_stream_scheduling_entry {
	diva_entity_link_t  link;
	struct _diva_stream *diva_stream;
	diva_stream_state_t diva_stream_state;
	struct capi_pvt      *i;
	int									rx_flow_control;
	int									tx_flow_control;
	char vname[CAPI_MAX_STRING]; /* Cached from capi_pvt */
	dword               PLCI; /* Cached from capi_pvt */
	time_t              cancel_start;
} diva_stream_scheduling_entry_t;

#endif

