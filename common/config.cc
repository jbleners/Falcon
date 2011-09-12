#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <yajl/yajl_parse.h>

#include <map>
#include <string>

namespace {

std::string next_key_;
std::map<std::string, char> key_type_;
bool started_map_ = false;
const int32_t kBufferSize = 4096;

int
handle_null(void *ctx) {
    // Nothing to do, map will map a null.
    return 1;
}

int
handle_bool(void *ctx, int v) {
    Config::dont_touch::config_[next_key_] = static_cast<void *>(new bool(v));
    key_type_[next_key_] = 'b';
    return 1;
}

int
handle_int(void *ctx, long long v) {
    Config::dont_touch::config_[next_key_] = static_cast<void *>(new long(v));
    key_type_[next_key_] = 'i';
    return 1;
}

int
handle_double(void *ctx, double v) {
    Config::dont_touch::config_[next_key_] = static_cast<void *>(new double(v));
    key_type_[next_key_] = 'd';
    return 1;
}

int
handle_str(void * ctx, const unsigned char * st, size_t st_len) {
    Config::dont_touch::config_[next_key_] =
        static_cast<void *> (
                new std::string(reinterpret_cast<const char *>(st), st_len));
    key_type_[next_key_] = 's';
    return 1;
}

int
map_start(void *ctx) {
    if (started_map_) {
        fprintf(stderr, "Config currently does not support nested maps.");
        exit(1);
    }
    started_map_ = true;
    return 1;
}

int
map_key(void *ctx, const unsigned char * key, size_t st_len) {
    next_key_ = std::string(reinterpret_cast<const char *>(key), st_len);
    return 1;
}

int
map_end(void *ctx) {
    return 1;
}

int
array_start(void *ctx) {
    fprintf(stderr, "Configuration currently does not support lists.");
    exit(1);
    return 1;
}

int
array_end(void *ctx) {
    fprintf(stderr, "Configuration currently does not support lists.");
    exit(1);
    return 1;
}

// What to do when type is encountered
yajl_callbacks cbs = {
    handle_null,
    handle_bool,
    handle_int,
    handle_double,
    NULL,
    handle_str,
    map_start,
    map_key,
    map_end,
    array_start,
    array_end
};

}  // end anonymous namespace

namespace Config {

namespace dont_touch {
std::map<std::string, void*> config_;
}

void
LoadConfig(const char* config_file) {
    unsigned char buf[kBufferSize];
    yajl_handle h = yajl_alloc(&cbs, NULL, NULL);
    FILE* f = fopen(config_file, "r");
    int buf_size;
    do {
        buf_size = fread(buf, sizeof(*buf), kBufferSize, f);
        assert(yajl_status_ok == yajl_parse(h, buf, buf_size));
    } while (buf_size == kBufferSize);
    assert(yajl_status_ok == yajl_complete_parse(h));
    yajl_free(h);
    fclose(f);
    return;
}

void
DeleteConfig() {
    std::map<std::string, void*>::iterator it;
    for (it = dont_touch::config_.begin(); it != dont_touch::config_.end();
         ++it) {
        switch (key_type_[it->first]) {
            case 'b':
                delete (static_cast<bool *>(it->second));
                break;
            case 'i':
                delete (static_cast<int *>(it->second));
                break;
            case 'd':
                delete (static_cast<double *>(it->second));
                break;
            case 's':
                delete (static_cast<std::string *>(it->second));
                break;
            case 0:
                break;  // NULL was keyed in by a default value
            default:
                fprintf(stderr, "Unknown type %c in config map key: %s",
                        key_type_[it->first], it->first.c_str());
                break;
        }
    }
    return;
}

}  // end namespace Config

#ifdef TEST_CONFIG
int
main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s config_file\n", argv[0]);
        exit(1);
    }
    Config::LoadConfig(argv[1]);
    std::string st;
    double d;
    bool b;
    assert(Config::GetFromConfig("idiot", &b));
    if (b) {
        assert(Config::GetFromConfig<double>("temp", &d));
        assert(Config::GetFromConfig<std::string>("name", &st));
        printf("%s has a temp of %f\n", st.c_str(), d);
    }
    Config::GetFromConfig("message", &st,
                          static_cast<std::string>("Hello world!"));
    printf("%s\n", st.c_str());
    Config::DeleteConfig();
    return 0;
}
#endif
