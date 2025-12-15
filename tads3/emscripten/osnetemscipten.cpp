#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <emscripten/emscripten.h>
#include <stdio.h>

extern "C"{

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
	printf("a%s\n", a);
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

int __wrap_socket(int domain, int type, int protocol){
	//net_work = emscripten_malloc_wasm_worker(1024);
  	//emscripten_wasm_worker_post_function_i(worker, __real_socket);
	return 4711;
}

}