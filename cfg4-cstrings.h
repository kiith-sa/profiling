//          Copyright Ferdinand Majerech 2014.
//    (See accompanying file LICENSE_1_0.txt or copy at
// Distributed under the Boost Software License, Version 1.0.
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CFG4_CSTRINGS_H_BYAOEKNU
#define CFG4_CSTRINGS_H_BYAOEKNU

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>


const std::string SPACES     = " \t";
const std::string COMMENTS   = ";#";
const std::string SEPARATORS = "=";

// Elements 9 and 32 (\t and ' ') are true; the rest are false (even after the first 32)
bool SPACES_LOOKUP[256] =
    {0, 0, 0, 0, 0, 0, 0, 0,
     0, 1, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,
     1};

/** This version improves on the previous by using plain C strings to avoid std::string 
 * overhead.
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
    while(!slice.empty() && SPACES_LOOKUP[slice.front()])
    {
        slice.pop_front();
    }
    while(!slice.empty() && SPACES_LOOKUP[slice.back()])
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
    /* std::vector<std::pair<std::string, std::string>> entries; */
    std::vector<std::pair<const char*, const char*>> entries;

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
            const auto key_slice = trim(slice.subslice(0, separator_idx));
            const auto val_slice = trim(slice.subslice(separator_idx + 1));

            // Copy from the (temporary) line to new C strings to store.
            char* const key   = new char[key_slice.size()   + 1];
            char* const value = new char[val_slice.size() + 1];
            memcpy(key, key_slice.ptr(), key_slice.size() * sizeof(char));
            key[key_slice.size()] = '\0';
            memcpy(value, val_slice.ptr(), val_slice.size() * sizeof(char));
            value[val_slice.size()] = '\0';

            entries.push_back(std::pair<const char*, const char*>(key, value));
        }

        // Sort entries by keys
        std::sort(entries.begin(), entries.end(),
                  [](const std::pair<const char*, const char*>& a,
                     const std::pair<const char*, const char*>& b) {
            return strcmp(a.first, b.first) < 0;
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

    // Need a manual destructor to delete our strings.
    //
    // The const char* pointers themselves will be deleted in the entries vector destructor.
    ~CFG()
    {
        for(auto key_value: entries)
        {
            delete[] key_value.first;
            delete[] key_value.second;
        }
    }

    // Need to copy our strings in the copy constructor; otherwise any copy of CFG would
    // delete our keys/values when destroyed.
    CFG(const CFG& other)
    {
        entries.reserve(other.entries.size());
        for(auto entry: other.entries)
        {
            const size_t key_len = strlen(entry.first) + 1;
            const size_t val_len = strlen(entry.second) + 1;
            auto key_copy = new char[key_len];
            auto val_copy = new char[val_len];
            memcpy(key_copy, entry.first, key_len * sizeof(char));
            memcpy(val_copy, entry.first, val_len * sizeof(char));
            entries.push_back(std::pair<const char*, const char*>(key_copy, val_copy));
        }
    }

    // swap(), operator= and CFG(CFG&&) are used to implement the copy-and-swap idiom
    // (see http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom)
    friend void swap(CFG& first, CFG& second) noexcept
    {
        first.entries.swap(second.entries);
        std::swap(first.valid, second.valid);
    }

    // Move constructor (google it)
    CFG(CFG&& other) : CFG() { swap(*this, other); }

    // Assignment - when assigning from a temporary, our data is swapped to it and destroyed
    // in the temporary's dtor.
    CFG& operator=(CFG other)
    {
        swap(*this, other);
        return *this;
    }


    bool is_valid() const
    {
        return valid;
    }

    /* auto find(const std::string& key) const -> decltype(entries.end()) */
    auto find(const char* const key) const -> decltype(entries.end())
    {
        assert(valid);
        // Find the first element *greater or equal* than key using binary search.
        auto lower_bound = std::lower_bound(entries.begin(), entries.end(), key,
            [](const std::pair<const char*, const char*>& a, const char* const b) {
            return strcmp(a.first, b) < 0;
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

#endif /* end of include guard: CFG4_CSTRINGS_H_BYAOEKNU */
