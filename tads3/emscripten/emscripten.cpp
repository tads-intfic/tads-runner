#include <stdio.h>
#include <stdlib.h>
#include <emscripten/emscripten.h>

extern "C"{

extern char *__real_fgets(char *str, int num, FILE *stream);
extern int __real_fflush(FILE *stream);

EM_JS(void, _flushstdout, (), {
    window._STDIO._flushstdout();
});

EM_JS(void, _flushstderr, (), {
    window._STDIO._flushstderr();
});

EM_ASYNC_JS(void, _wait_for_stdin, (), {
    await window._STDIO._flushstdin();
});

char *__wrap_fgets(char * str, int num, FILE * stream) {
    _wait_for_stdin();
    return __real_fgets(str, num, stream);
}

int __wrap_fflush(FILE *stream) {
    int ret = __real_fflush(stream);
    if (stream == stdout) {
        _flushstdout();
    } else if (stream == stderr) {
        _flushstderr();
    }
    return ret;
}

}
