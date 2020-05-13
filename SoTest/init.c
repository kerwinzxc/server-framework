#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "mq_cmd.h"
#include "mq_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static int centerChannelHeartbeat(Channel_t* c, int heartbeat_times) {
	if (heartbeat_times < c->heartbeat_maxtimes) {
		SendMsg_t msg;
		makeSendMsgEmpty(&msg);
		channelSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		printf("channel(%p) send heartbeat, times %d...\n", c, heartbeat_times);
	}
	else {
		ReactorCmd_t* cmd;
		printf("channel(%p) zombie...\n", c);
		cmd = reactorNewReuseCmd(&c->_, NULL);
		if (!cmd) {
			return 0;
		}
		reactorCommitCmd(NULL, cmd);
		printf("channel(%p) reconnect start...\n", c);
	}
	return 1;
}

static void centerChannelConnectCallback(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	char buffer[1024];
	SendMsg_t msg;
	IPString_t peer_ip = { 0 };
	unsigned short peer_port = 0;

	channelEnableHeartbeat(channel, ts_msec);

	sockaddrDecode(&c->to_addr.st, peer_ip, &peer_port);
	printf("channel(%p) connect success, ip:%s, port:%hu\n", c, peer_ip, peer_port);

	if (c->connected_times > 1) {
		sprintf(buffer, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u,\"session_id\":%d}",
			ptr_g_ClusterSelf()->name, ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port, channelSessionId(channel));
		makeSendMsg(&msg, CMD_REQ_RECONNECT, buffer, strlen(buffer));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_SYN);
	}
	else if (channel->rpc_itemlist.head) {
		RpcItem_t* rpc_item = pod_container_of(channel->rpc_itemlist.head, RpcItem_t, listnode);
		UserMsg_t* msg = newUserMsg(0);
		msg->channel = channel;
		msg->rpcid = rpc_item->id;
		msg->rpc_status = 'T';
		dataqueuePush(&g_DataQueue, &msg->internal._);
	}
	else {
		sprintf(buffer, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u}",
			ptr_g_ClusterSelf()->name, ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port);
		makeSendMsg(&msg, CMD_REQ_UPLOAD_CLUSTER, buffer, strlen(buffer));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	}
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	int connectsockinitokcnt;

	set_g_DefaultDispatchCallback(unknowRequest);
	regNumberDispatch(CMD_REQ_TEST, reqTest);
	regNumberDispatch(CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(CMD_RET_TEST, retTest);
	regNumberDispatch(CMD_REQ_RECONNECT, reqReconnectCluster);
	regNumberDispatch(CMD_RET_RECONNECT, retReconnect);
	regNumberDispatch(CMD_REQ_UPLOAD_CLUSTER, reqUploadCluster);
	regNumberDispatch(CMD_RET_UPLOAD_CLUSTER, retUploadCluster);
	regNumberDispatch(CMD_NOTIFY_NEW_CLUSTER, notifyNewCluster);
	regNumberDispatch(CMD_REQ_REMOVE_CLUSTER, reqRemoveCluster);
	regNumberDispatch(CMD_RET_REMOVE_CLUSTER, retRemoveCluster);
	regStringDispatch("/reqHttpTest", reqHttpTest);
	regStringDispatch("/reqSoTest", reqSoTest);

	for (connectsockinitokcnt = 0; connectsockinitokcnt < ptr_g_Config()->connect_options_cnt; ++connectsockinitokcnt) {
		ConfigConnectOption_t* option = ptr_g_Config()->connect_options + connectsockinitokcnt;
		Sockaddr_t connect_addr;
		Channel_t* c;
		ReactorObject_t* o;
		if (strcmp(option->protocol, "inner")) {
			continue;
		}
		if (!sockaddrEncode(&connect_addr.st, ipstrFamily(option->ip), option->ip, option->port))
			return 0;
		o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, option->socktype, 0);
		if (!o)
			return 0;
		c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
		if (!c) {
			reactorCommitCmd(NULL, &o->freecmd);
			return 0;
		}
		if (!strcmp(option->protocol, "inner")) {
			c->_.on_syn_ack = centerChannelConnectCallback;
			c->on_heartbeat = centerChannelHeartbeat;
		}
		printf("channel(%p) connecting......\n", c);
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
	}

	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif