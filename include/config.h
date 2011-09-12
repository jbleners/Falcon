#ifndef _NTFA_INCLUDE_CONFIG_H
#define _NTFA_INCLUDE_CONFIG_H

#include <string>
#include <map>

namespace Config {

// LoadConfig loads a JSON configuration file from config_path and parses
// it into a config object which can be accessed with GetFromConfig.
extern void LoadConfig(const char *config_path);
extern void DeleteConfig();

// Ugh, don't touch. Templates didn't work quite how I thought and how I have
// to export this so my generated functions can use it
namespace dont_touch {
extern std::map<std::string,void*> config_;
}
// Puts copies config object of type T into value_ptr.
// Returns true if key exists, false if value is null

template <typename T>
inline bool
GetFromConfig(const std::string& key, T* out) {
    void *val = dont_touch::config_[key];
    if (val) {
        *out = *static_cast<T*>(val);
        return true;
    } else {
        return false;
    }
}

template <typename T>
inline bool
GetFromConfig(const char *key, T* out) {
    std::string my_key(key);
    return GetFromConfig(my_key, out);
}

template <typename T>
inline void
GetFromConfig(const std::string& key, T* out, T& default_value) {
    if (GetFromConfig(key, out)) {
        return;
    }
    *out = default_value;
    return;
}

template <typename T>
inline void
GetFromConfig(const char* key, T* out, T& default_value) {
    if (GetFromConfig(key, out)) {
        return;
    }
    *out = default_value;
    return;
}

template <typename T>
inline void
GetFromConfig(const std::string& key, T* out, T default_value) {
    if (GetFromConfig(key, out)) {
        return;
    }
    *out = default_value;
    return;
}

template <typename T>
inline void
GetFromConfig(const char* key, T* out, T default_value) {
    if (GetFromConfig(key, out)) {
        return;
    }
    *out = default_value;
    return;
}


} // end namespace config
#endif // _NTFA_INCLUDE_CONFIG_H
