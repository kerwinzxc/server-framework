#ifndef DISPATCH_H
#define	DISPATCH_H

#include "util/inc/component/channel.h"
#include "util/inc/component/httpframe.h"
#include "work_thread.h"

typedef struct UserMsg_t {
	ReactorCmd_t internal;
	Channel_t* channel;
	Sockaddr_t peer_addr;
	HttpFrame_t* httpframe;
	const char* cmdstr;
	char rpc_status;
	union {
		int cmdid;
		int retcode;
	};
	int rpcid;
	size_t datalen;
	unsigned char data[1];
} UserMsg_t;

typedef void(*DispatchCallback_t)(TaskThread_t*, UserMsg_t*);

extern DispatchCallback_t g_DefaultDispatchCallback;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport UserMsg_t* newUserMsg(size_t datalen);

__declspec_dllexport void set_g_DefaultDispatchCallback(DispatchCallback_t fn);

int initDispatch(void);
__declspec_dllexport int regStringDispatch(const char* str, DispatchCallback_t func);
__declspec_dllexport int regNumberDispatch(int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(const char* str);
DispatchCallback_t getNumberDispatch(int cmd);
void freeDispatchCallback(void);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
