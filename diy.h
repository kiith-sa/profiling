//          Copyright Ferdinand Majerech 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <time.h> 
#include <cstdint>
#include <iostream>

// POSIX ONLY! For portability add a Windows get_nsecs() implementation and use #ifdef

/// Gets current time in nanoseconds
uint64_t get_nsecs()
{
    timespec ts;
    // If this fails, we can't profile on this machine
    if(clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        return 0;
    }

    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/// Set to true to print zone times
static bool PRINT_ZONES = false;

/// Measures time spent between its constructor and destructor.
class Zone
{
private:
    const uint64_t start;

    const char* const name;

public:
    Zone(const char* const name)
        : start(get_nsecs())
        , name(name)
    {}

    ~Zone()
    {
        const uint64_t end = get_nsecs();
        const uint64_t dur = end - start;
        if(PRINT_ZONES)
        {
            std::cout << "Zone '" << name << "':\n" 
                << "\t" << dur << " ns from " << start << " to " << end << "\n";
        }
    }
};
