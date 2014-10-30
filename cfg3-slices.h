//          Copyright Ferdinand Majerech 2014.
//    (See accompanying file LICENSE_1_0.txt or copy at
// Distributed under the Boost Software License, Version 1.0.
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CFG3_SLICES_H_JW47MYPP
#define CFG3_SLICES_H_JW47MYPP

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

const std::string SPACES     = " \t";
const std::string COMMENTS   = ";#";
const std::string SEPARATORS = "=";


/** This version uses STL-compatible generic 'slices' to avoid string construction.
 * 
 * It is supposed to be the fastest 'reasonable' version without using plain C strings.
 *
 * Without inlining, this is slower than the previous version due to a huge number of
 * small function calls. With inlining, it is faster.
 */


/** A slice of a std::vector or std::string.
 *
 * References memory but does not own it. Provides a safe-ish view into a string/vector.
 */
template<typename T>
class Slice
{
private:
    const T* ptr_;
    size_t size_;

public:
    /// Construct a slice of all data in a std::string or std::vector.
    template<typename A>
    Slice(const A& array)
    {
        ptr_  = array.data();
        size_ = array.size();
    }

    /// Raw slice constructor from a pointer and a size.
    Slice(const T* ptr, const size_t size)
    {
        ptr_    = ptr;
        size_ = size;
    }

    /// Get a subslice() - like std::string::substr() but without allocation.
    Slice subslice(const size_t start, const size_t size) const
    {
        assert(start + size <= size_);
        return Slice(ptr_ + start, size);
    }

    /// Ditto.
    Slice subslice(const size_t start) const
    {
        assert(start <= size_);
        return Slice(ptr_ + start, size_ - start);
    }

    /// True if there are no elements in the slice.
    bool empty() const { return size_ == 0; }

    /// Get the front (first) element.
    T front() const
    {
        assert(!empty());
        return *ptr_; 
    }

    /// Get the back (last) element.
    T back() const
    {
        assert(!empty());
        return *(ptr_ + size_ - 1);
    }

    /// Remove the front (first) element.
    void pop_front()
    {
        assert(!empty());
        ++ptr_;
        --size_;
    }

    /// Remove the back (last) element.
    void pop_back()
    {
        assert(!empty());
        --size_;
    }

    /// Get the pointer to the first element of the slice.
    const T* ptr() const { return ptr_; }

    /// Get the pointer *after* the last element of the slice (for STL <algorithm>).
    const T* end() const { return ptr_ + size_; }

    /// Get the number of elements in the slice.
    const size_t size() const { return size_; }
};

/// Trim without any allocation or string construction.
Slice<char> trim(Slice<char> slice)
{
    while(!slice.empty() && SPACES.find(slice.front()) != std::string::npos)
    {
        slice.pop_front(); 
    }
    while(!slice.empty() && SPACES.find(slice.back())  != std::string::npos) 
    {
        slice.pop_back(); 
    }
    return slice;
}

/// Simple CFG file with no sections, implemented on top of std::map.
class CFG
{
private:
    // Vector of key-value pairs sorted by keys
    std::vector<std::pair<std::string, std::string>> entries;

    bool valid = true;

public:
    CFG():valid(false) {}

    CFG(const std::string& filename) noexcept
    {
        std::ifstream file(filename);
        if(!file.good())
        {
            valid = false;
            return;
        }

        // Read the file by line
        std::string line;
        while(std::getline(file, line))
        {
            // Strip comments
            auto slice = Slice<char>(line);
            const char* comment_ptr = std::find_first_of(slice.ptr(), slice.end(),
                                                         COMMENTS.begin(), COMMENTS.end());
            if(comment_ptr != slice.end())
            {
                slice = slice.subslice(0, comment_ptr - slice.ptr());
            }

            // Ignore blank and empty lines
            slice = trim(slice);
            if(slice.empty())
            {
                continue;
            }

            // Separate into key and value
            const char* separator_ptr = std::find_first_of(slice.ptr(), slice.end(),
                                                           SEPARATORS.begin(), SEPARATORS.end());
            if(separator_ptr == slice.end())
            {
                // Treat non-empty lines with separators as errors
                std::cerr << "ERROR: non-empty line with no separator in "
                          << filename << ": " << std::string(slice.ptr(), slice.size())
                          << std::endl;
                valid = false;
                return;
            }

            const size_t separator_idx = separator_ptr - slice.ptr();
            const auto key   = trim(slice.subslice(0, separator_idx));
            const auto value = trim(slice.subslice(separator_idx + 1));

            entries.push_back(std::pair<std::string, std::string>
                              (std::string(key.ptr(), key.size()),
                               std::string(value.ptr(), value.size())));
        }

        // Sort entries by keys
        std::sort(entries.begin(), entries.end(),
                  [](const std::pair<std::string, std::string>& a,
                     const std::pair<std::string, std::string>& b) {
            return a.first < b.first;
        });

        // Check for duplicates after sorting.
        bool first_key = true;
        std::string prev_key;
        for(auto& key_value: *this)
        {
            if(!first_key && prev_key == key_value.first)
            {
                // Treat duplicate keys as errors
                std::cerr << "ERROR: Duplicate key in " << filename << ": " << prev_key
                          << std::endl;
                valid = false;
                return;
            }
            else
            {
                first_key = false;
            }
            prev_key = key_value.first;
        }
    }

    bool is_valid() const
    {
        return valid;
    }

    auto find(const std::string key) const -> decltype(entries.end())
    {
        assert(valid);
        // Find the first element *greater or equal* than key using binary search.
        auto lower_bound = std::lower_bound(entries.begin(), entries.end(), key,
            [](const std::pair<std::string, std::string>& a, const std::string& b) {
            return a.first < b;
        });

        // If equal, we've found the key.
        if(lower_bound->first == key)
        {
            return lower_bound;
        }

        // If greater, no such key in entries.
        return entries.end();
    }

    auto begin() const -> decltype(entries.begin())
    {
        assert(valid);
        return entries.begin();
    }

    auto end() const -> decltype(entries.begin())
    {
        assert(valid);
        return entries.end();
    }

    size_t size() const
    {
        assert(valid);
        return entries.size();
    }
};

#endif /* end of include guard: CFG3_SLICES_H_JW47MYPP */
