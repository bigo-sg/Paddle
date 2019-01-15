#ifndef FAKE_GLOG_H
#define FAKE_GLOG_H

#include <ostream>
#include <gflags/gflags.h>

namespace fake_glog {

struct NullStream : std::ostream {
    NullStream() : std::ostream(nullptr) {}
    NullStream(const NullStream&) = delete;
    NullStream& operator=(const NullStream&) = delete;
};

} //fake_glog

#define LOG(severity) ::fake_glog::NullStream()
#define VLOG(severity) ::fake_glog::NullStream()
#define VLOG_IS_ON(severity) (false)
#define CHECK(condition) ::fake_glog::NullStream()

namespace google {
static inline void InitGoogleLogging(const char*) {}
static inline void InstallFailureSignalHandler() {}
} //google

#endif //FAKE_GLOG_H
