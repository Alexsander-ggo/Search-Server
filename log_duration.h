#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(id) LogDuration UNIQUE_VAR_NAME_PROFILE(id)
#define LOG_DURATION_STREAM(id, stream) LogDuration UNIQUE_VAR_NAME_PROFILE(id, stream)

class LogDuration {   
public:
    using                           Clock = std::chrono::steady_clock;

                                    LogDuration(const std::string& id, std::ostream& stream = std::cerr); 
    
                                    ~LogDuration(); 
    
private: 
    const std::string               id_;
    const Clock::time_point         start_time_ = Clock::now();
    std::ostream&                   stream_;
};

LogDuration::LogDuration(const std::string& id, std::ostream& stream)
    : id_(id)
    , stream_(stream) {}

LogDuration::~LogDuration() {
    using namespace std;
    using namespace std::chrono;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    stream_ << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << endl;
}