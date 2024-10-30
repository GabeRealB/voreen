/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#ifndef VRN_MA_MEMORY_MAPPED_FILE_H
#define VRN_MA_MEMORY_MAPPED_FILE_H

#include <cstdio>
#include <string>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32

#include <voreen/core/utils/exception.h>
#include "tgt/logmanager.h"

namespace voreen {

template <typename T>
struct MappedRange {
    T* start_ptr;
    T* end_ptr;
};

class ReadOnlyMappedFile final {
public:
    explicit ReadOnlyMappedFile(const std::string& path);
    ReadOnlyMappedFile(const ReadOnlyMappedFile&) = delete;
    ReadOnlyMappedFile(ReadOnlyMappedFile&&) noexcept;
    ~ReadOnlyMappedFile() noexcept;

    ReadOnlyMappedFile& operator=(const ReadOnlyMappedFile&) = delete;
    ReadOnlyMappedFile& operator=(ReadOnlyMappedFile&&) noexcept;

    friend void swap(ReadOnlyMappedFile& a, ReadOnlyMappedFile& b) noexcept {
        using std::swap;

#ifdef _WIN32
        swap(a.m_buffer, b.m_buffer);
        swap(a.m_file, b.m_file);
        swap(a.m_mapping, b.m_mapping);
        swap(a.m_size, b.m_size);
#else
        swap(a.m_buffer, b.m_buffer);
        swap(a.m_size, b.m_size);
        swap(a.m_fd, b.m_fd);
#endif
    }

    const char* data() const noexcept { return static_cast<const char*>(this->m_buffer); }
    std::size_t size() const noexcept { return this->m_size; }

    template <typename T>
    const T* at(const std::size_t cursor) const {
        if (cursor > this->size() || cursor + sizeof(T) > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadOnlyMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadOnlyMappedFile: Cursor not aligned!");
        }

        return reinterpret_cast<const T*>(this->data() + cursor);
    }

    template <typename T>
    MappedRange<const T> at(const std::size_t cursor, const std::size_t size) const {
        const std::size_t cursor_end = cursor + size * sizeof(T);

        if (cursor > this->size() || cursor_end > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadOnlyMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadOnlyMappedFile: Cursor not aligned!");
        }

        const T* start_ptr = reinterpret_cast<const T*>(this->data() + cursor);
        const T* end_ptr = reinterpret_cast<const T*>(this->data() + cursor_end);

        return MappedRange<const T>{start_ptr, end_ptr};
    }

private:
    static constexpr const char* LOGGER_CAT = "ReadOnlyMappedFile";

#ifdef _WIN32
    const LPVOID m_buffer;
    HANDLE m_file;
    HANDLE m_mapping;
    std::size_t m_size;
#else
    const void* m_buffer;
    std::size_t m_size;
    int m_fd;
#endif // _WIN32
};

class ReadWriteMappedFile final {
public:
    explicit ReadWriteMappedFile(const std::string& path);
    ReadWriteMappedFile(const ReadWriteMappedFile&) = delete;
    ReadWriteMappedFile(ReadWriteMappedFile&&) noexcept;
    ~ReadWriteMappedFile() noexcept;

    ReadWriteMappedFile& operator=(const ReadWriteMappedFile&) = delete;
    ReadWriteMappedFile& operator=(ReadWriteMappedFile&&) noexcept;

    friend void swap(ReadWriteMappedFile& a, ReadWriteMappedFile& b) noexcept {
        using std::swap;

#ifdef _WIN32
        swap(a.m_buffer, b.m_buffer);
        swap(a.m_file, b.m_file);
        swap(a.m_mapping, b.m_mapping);
        swap(a.m_size, b.m_size);
#else
        swap(a.m_buffer, b.m_buffer);
        swap(a.m_size, b.m_size);
        swap(a.m_fd, b.m_fd);
#endif
    }

    char* data() noexcept { return static_cast<char*>(this->m_buffer); }
    const char* data() const noexcept { return static_cast<const char*>(this->m_buffer); }
    std::size_t size() const noexcept { return this->m_size; }
    void flush() const;

    template <typename T>
    std::size_t ensure_capacity(const std::size_t cursor) {
        return this->ensure_capacity_inner(cursor, sizeof(T), alignof(T));
    }

    template <typename T>
    std::size_t ensure_capacity(const std::size_t cursor, const std::size_t size) {
        return this->ensure_capacity_inner(cursor, size * sizeof(T), alignof(T));
    }

    template <typename T>
    T* at(const std::size_t cursor) {
        if (cursor > this->size() || cursor + sizeof(T) > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadWriteMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadWriteMappedFile: Cursor not aligned!");
        }

        return reinterpret_cast<T*>(this->data() + cursor);
    }

    template <typename T>
    const T* at(const std::size_t cursor) const {
        if (cursor > this->size() || cursor + sizeof(T) > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadWriteMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadWriteMappedFile: Cursor not aligned!");
        }

        return reinterpret_cast<const T*>(this->data() + cursor);
    }

    template <typename T>
    MappedRange<T> at(const std::size_t cursor, const std::size_t size) {
        const std::size_t cursor_end = cursor + size * sizeof(T);

        if (cursor > this->size() || cursor_end > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadWriteMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadWriteMappedFile: Cursor not aligned!");
        }

        const T* start_ptr = reinterpret_cast<T*>(this->data() + cursor);
        const T* end_ptr = reinterpret_cast<T*>(this->data() + cursor_end);

        return MappedRange<T>{start_ptr, end_ptr};
    }

    template <typename T>
    MappedRange<const T> at(const std::size_t cursor, const std::size_t size) const {
        const std::size_t cursor_end = cursor + size * sizeof(T);

        if (cursor > this->size() || cursor_end > this->size()) {
            LERRORC(LOGGER_CAT, "Cursor out of bounds!");
            throw VoreenException("ReadWriteMappedFile: Cursor out of bounds!");
        }
        if (cursor & alignof(T) != 0) {
            LERRORC(LOGGER_CAT, "Cursor not aligned!");
            throw VoreenException("ReadWriteMappedFile: Cursor not aligned!");
        }

        const T* start_ptr = reinterpret_cast<const T*>(this->data() + cursor);
        const T* end_ptr = reinterpret_cast<const T*>(this->data() + cursor_end);

        return MappedRange<const T>{start_ptr, end_ptr};
    }

private:
    static constexpr const char* LOGGER_CAT = "ReadWriteMappedFile";

    std::size_t ensure_capacity_inner(std::size_t cursor, std::size_t size, std::size_t alignment);

#ifdef _WIN32
    LPVOID m_buffer;
    HANDLE m_file;
    HANDLE m_mapping;
    std::size_t m_size;
#else
    void* m_buffer;
    std::size_t m_size;
    int m_fd;
#endif // _WIN32
};

} // namespace voreen

#endif // VRN_MA_MEMORY_MAPPED_FILE_H
