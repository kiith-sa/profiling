//          Copyright Ferdinand Majerech 2014.
//    (See accompanying file LICENSE_1_0.txt or copy at
// Distributed under the Boost Software License, Version 1.0.
//          http://www.boost.org/LICENSE_1_0.txt)


#ifndef CFG5_NOALLOC_H_VIXAU86T
#define CFG5_NOALLOC_H_VIXAU86T

#include <algorithm>
#include <cassert>
#include <cstddef>
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

/** This version avoids most allocations by loading the entire file to a single buffer
 * and making all strings point into that buffer.
 */


/** A slice of a std::vector or std::string.
 *
 * References memory but does not own it. Provides a safe-ish view into a string/vector.
 */
template<typename T>
class Slice
{
private:
    T* ptr_;
    size_t size_;

public:
    /// Copy constructor.
    Slice(const Slice<T>& rhs)
    {
        ptr_  = rhs.ptr_;
        size_ = rhs.size_;
    }

    /// Copy constructor from a non-const reference (needed to shadow the main constructor).
    Slice(Slice<T>& rhs)
    {
        ptr_  = rhs.ptr_;
        size_ = rhs.size_;
    }

    /// Construct a slice of all data in a std::string or std::vector.
    template<typename A>
    Slice(A& array)
    {
        ptr_  = array.data();
        size_ = array.size();
    }


    /// Raw slice constructor from a pointer and a size.
    Slice(T* ptr, const size_t size)
    {
        ptr_  = ptr;
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

    /// Get a non-const pointer to the first element of the slice.
    T* ptr_mutable() const { return ptr_; }

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
    // Vector of key-value pairs sorted by keys.
    //
    // Both key and value strings point to the storage vector instead of owning their data.
    std::vector<std::pair<const char*, const char*>> entries;

    // The entire file is loaded here.
    std::vector<char> storage;

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

        // Seek to end of file
        file.seekg(0, std::ios::end);
        // Resize to file size (+ 1 char for zero terminator)
        storage.resize(static_cast<size_t>(file.tellg()) + 1);
        // Return to beginning of file
        file.seekg(0, std::ios::beg);
        // Read the entire file
        file.read(storage.data(), storage.size());
        // Set the last element of storage to '\0', making storage a zero-terminated string
        storage.back() = '\0';
        file.close();

        const char* const newlines = "\r\n";

        // Tokenize storage using newlines as delimiters.
        // strtok() will replace the delimiters in storage with '\0', so the returned
        // tokens (tok) will be C strings pointing to storage ending before the (replaced)
        // delimiters.
        for(char* tok = strtok(storage.data(), newlines);
            NULL != tok;
            tok = strtok(NULL, newlines))
        {
            // Strip comments
            auto slice = Slice<char>(tok, strlen(tok));
            // auto slice = Slice<char>(line);
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

            // No allocations or copying; use pointers to the loaded file.
            char* key   = key_slice.ptr_mutable();
            char* value = val_slice.ptr_mutable();
            // This is safe because the key is followed (at least) by a separator
            // and the value is followed (at least) by the end of line.
            key[key_slice.size()]   = '\0';
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
        const char* prev_key;
        for(auto& key_value: *this)
        {
            if(!first_key && 0 == strcmp(prev_key, key_value.first))
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

    // storage destructor is called automatically.
    ~CFG()
    {
    }

    // Need to copy our strings in the copy constructor; otherwise any copy of CFG would
    // delete our keys/values when destroyed.
    CFG(const CFG& other)
    {
        storage = other.storage;
        entries.reserve(other.entries.size());
        for(auto entry: other.entries)
        {
            // Both key and value point to storage; they don't own their data.
            // Our storage is a copy of other.storage, so we determine the positions of
            // keys/values in storage and initialize our keys/values based on that.
            const ptrdiff_t key_offset = entry.first - other.storage.data();
            const ptrdiff_t val_offset = entry.second - other.storage.data();
            const char* key   = storage.data() + key_offset;
            const char* value = storage.data() + val_offset;
            entries.push_back(std::pair<const char*, const char*>(key, value));
        }
    }

    // swap(), operator= and CFG(CFG&&) are used to implement the copy-and-swap idiom
    // (see http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom)
    friend void swap(CFG& first, CFG& second) noexcept
    {
        first.storage.swap(second.storage);
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


#endif /* end of include guard: CFG5_NOALLOC_H_VIXAU86T */
