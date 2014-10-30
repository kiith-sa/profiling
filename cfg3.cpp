//          Copyright Ferdinand Majerech 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>
#include <vector>

#include "cfg3-slices.h"
#include "diy.h"


int main(int argc, const char* const argv[])
{
    // PRINT_ZONES = true;

    if(argc < 3)
    {
        std::cerr << "ERROR: need args. " << std::endl;
        std::cerr << "Example: ./cfg huge.cfg 2 " << std::endl;
        return 1;
    }

    const char* const filename = argv[1];

    unsigned times;
    try
    {
        times = std::stoul(argv[2]);
    }
    catch(...)
    {
        std::cerr << "ERROR: second arg must be a number" << std::endl;
        return 1;
    }

    // Simulates work when randomly accessing strings;
    std::string workDummy;


    CFG cfg;
    for(unsigned t = 0; t < times; ++t)
    {
        {
            Zone zone("parsing");
            cfg = CFG(filename);
        }

        if(!cfg.is_valid())
        {
            std::cerr << "ERROR: Failed to open or parse file " << filename << std::endl;
            return 1;
        }

        std::vector<std::string> keys;
        keys.reserve(cfg.size());

        {
            Zone zone("iteration");
            for(auto& key_value: cfg)
            {
                keys.push_back(key_value.first);
            }
        }

        {
            Zone zone("random access");
            for(const std::string& key: keys)
            {
                const auto found = cfg.find(key);
                // assert(key == found->first);
                workDummy = key + "=" + found->second ;
            }
        }

        // Test that find() does not accidentally find something not in the config file
        assert(cfg.find("<<<NOT=HERE>>>") == cfg.end());
    }

    // Ensures workDummy is not optimized away
    std::cout << workDummy << std::endl;

    return 0;
}


