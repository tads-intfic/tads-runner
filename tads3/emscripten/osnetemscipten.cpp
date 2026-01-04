#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/websocket.h>
#include <emscripten/posix_socket.h>
#include <stdio.h>

extern "C"{
	
static EMSCRIPTEN_WEBSOCKET_T bridgeSocket = 0;

struct hostent *gethostbyname(const char *name){
	static struct hostent ent;
	//static char ip[4] = {127, 0, 0, 1};
	//static char* ipaddr = &ip;
	static in_addr_t ip = inet_addr("127.0.0.1");
	char* ipptr = (char*)&ip;
	ent.h_name = (char*)"emscripten";
	ent.h_length = 1;
	ent.h_addr_list = (char**)&ipptr;
	//why do we need this? without it, it will segfault
	const char *a =
            inet_ntoa(*(struct in_addr *)ent.h_addr_list[0]);
	return &ent;
}

extern int __real_socket(int domain, int type, int protocol);

/*static emscripten_wasm_worker_t net_work = nullptr;

EM_JS(int, call_real_socket, (int domain, int type, int protocol), {
	Module.ccall("real_socket", "number", ["number", "number", "number"], [domain, type, protocol], {async: true}).then(result => {
		return result;
	});
});

int real_socket(int domain, int type, int protocol){
	return __real_socket(domain, type, protocol);
}*/

static bool ws_init = false;

int __wrap_socket(int domain, int type, int protocol){
	if (!ws_init){
		printf("Hallo\n");
		bridgeSocket = emscripten_init_websocket_to_posix_socket_bridge("ws://localhost:8888");
		// Synchronously wait until connection has been established.
		uint16_t readyState = 0;
		do {
			emscripten_websocket_get_ready_state(bridgeSocket, &readyState);
			emscripten_thread_sleep(100);
			printf("Connecting\n");
		} while (readyState == 0);
		printf("Connected\n");
		ws_init = true;
	}
	//net_work = emscripten_malloc_wasm_worker(1024);
  	//emscripten_wasm_worker_post_function_i(worker, __real_socket);
	return __real_socket(domain, type, protocol);
}

}