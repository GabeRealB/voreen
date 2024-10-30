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

#include "memorymappedfile.h"

#include <limits>
#include <exception>

#ifdef _WIN32
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif // _WIN32

namespace voreen {

ReadOnlyMappedFile::ReadOnlyMappedFile(const std::string& path) {
#ifdef _WIN32
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        LERRORC(LOGGER_CAT, "Failed to open file!");
        throw VoreenException("ReadOnlyMappedFile: Failed to open file!");
    }

    ULARGE_INTEGER li {};
    li.LowPart = GetFileSize(file, &li.HighPart);
    if (li.LowPart == INVALID_FILE_SIZE) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "Failed to get file size!");
        throw VoreenException("ReadOnlyMappedFile: Failed to get file size!");
    }

    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadOnlyMappedFile: Failed to create file mapping!");
    }

    const LPVOID addr = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (addr == nullptr) {
        CloseHandle(mapping);
        CloseHandle(file);
        LERRORC(BINARY_FILE_LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadOnlyMappedFile: File mapping failed!");
    }

    this->m_buffer = addr;
    this->m_file = file;
    this->m_mapping = mapping;
    this->m_size = static_cast<std::size_t>(li.QuadPart);
#else
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        LERRORC(LOGGER_CAT, "Failed to open file!");
        throw VoreenException("ReadOnlyMappedFile: Failed to open file!");
    }

    struct stat st{};
    if (fstat(fd, &st) == -1) {
        close(fd);
        LERRORC(LOGGER_CAT, "Failed to stat file!");
        throw VoreenException("ReadOnlyMappedFile: Failed to stat file!");
    }

    if (st.st_size == 0) {
        close(fd);
        LERRORC(LOGGER_CAT, "File is empty!");
        throw VoreenException("ReadOnlyMappedFile: File is empty!");
    }

    const auto size = static_cast<std::size_t>(st.st_size);
    const void* data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadOnlyMappedFile: File mapping failed!");
    }

    this->m_buffer = data;
    this->m_size = size;
    this->m_fd = fd;
#endif // _WIN32
}

ReadOnlyMappedFile::ReadOnlyMappedFile(ReadOnlyMappedFile&& file) noexcept {
#ifdef _WIN32
    this->m_buffer = file.m_buffer;
    this->m_file = file.m_file;
    this->m_mapping = file.m_mapping;
    this->m_size = file.m_size;
    file.m_buffer = nullptr;
    file.m_file = nullptr;
    file.m_mapping = nullptr;
    file.m_size = 0;
#else
    this->m_buffer = file.m_buffer;
    this->m_size = file.m_size;
    this->m_fd = file.m_fd;
    file.m_buffer = nullptr;
    file.m_size = 0;
    file.m_fd = -1;
#endif // _WIN32
}

ReadOnlyMappedFile::~ReadOnlyMappedFile() noexcept {
#ifdef _WIN32
    if (this->m_buffer != nullptr) {
        UnmapViewOfFile(const_cast<LPVOID>(this->m_buffer));
        CloseHandle(this->m_mapping);
        CloseHandle(this->m_file);

        this->m_buffer = nullptr;
        this->m_file = nullptr;
        this->m_mapping = nullptr;
        this->m_size = 0;
    }
#else
    if (this->m_buffer != nullptr) {
        if (munmap(const_cast<void*>(this->m_buffer), this->m_size) == -1) {
            LERRORC(LOGGER_CAT, "File unmapping failed!");
            std::terminate();
        }
        close(this->m_fd);

        this->m_buffer = nullptr;
        this->m_size = 0;
        this->m_fd = -1;
    }
#endif // _WIN32
}

ReadOnlyMappedFile& ReadOnlyMappedFile::operator=(ReadOnlyMappedFile&& other) noexcept {
    using std::swap;

    if (this != &other) {
        swap(*this, other);
    }

    return *this;
}

ReadWriteMappedFile::ReadWriteMappedFile(const std::string& path) {
#ifdef _WIN32
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        LERRORC(LOGGER_CAT, "Failed to open file!");
        throw VoreenException("ReadWriteMappedFile: Failed to open file!");
    }

    ULARGE_INTEGER li {};
    li.QuadPart = 1;
    if (SetFilePointer(file, li.LowPart, &li.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "Failed to move the file pointer to the end of the file!");
        throw VoreenException("ReadWriteMappedFile: Failed to move the file pointer to the end of the file!");
    }

    if (!SetEndOfFile(file)) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "Failed to extend file!");
        throw VoreenException("ReadWriteMappedFile: Failed to extend file!");
    }

    if (SetFilePointer(file, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "Failed to move the file pointer to the start of the file!");
        throw VoreenException("ReadWriteMappedFile: Failed to move the file pointer to the start of the file!");
    }

    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (mapping == nullptr) {
        CloseHandle(file);
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: Failed to create file mapping!");
    }

    LPVOID addr = MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (addr == nullptr) {
        CloseHandle(mapping);
        CloseHandle(file);
        LERRORC(BINARY_FILE_LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: File mapping failed!");
    }

    this->m_buffer = addr;
    this->m_file = file;
    this->m_mapping = mapping;
    this->m_size = 1;
#else
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        LERRORC(LOGGER_CAT, "Failed to open file!");
        throw VoreenException("ReadWriteMappedFile: Failed to open file!");
    }

    if (ftruncate(fd, 1) == -1) {
        close(fd);
        LERRORC(LOGGER_CAT, "Failed to extend file!");
        throw VoreenException("ReadWriteMappedFile: Failed to extend file!");
    }

    void* data = mmap(nullptr, 1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: File mapping failed!");
    }

    this->m_buffer = data;
    this->m_size = 1;
    this->m_fd = fd;
#endif // _WIN32
}

ReadWriteMappedFile::ReadWriteMappedFile(ReadWriteMappedFile&& file) noexcept {
#ifdef _WIN32
    this->m_buffer = file.m_buffer;
    this->m_file = file.m_file;
    this->m_mapping = file.m_mapping;
    this->m_size = file.m_size;
    file.m_buffer = nullptr;
    file.m_file = nullptr;
    file.m_mapping = nullptr;
    file.m_size = 0;
#else
    this->m_buffer = file.m_buffer;
    this->m_size = file.m_size;
    this->m_fd = file.m_fd;
    file.m_buffer = nullptr;
    file.m_size = 0;
    file.m_fd = -1;
#endif // _WIN32
}

ReadWriteMappedFile::~ReadWriteMappedFile() noexcept {
#ifdef _WIN32
    if (this->m_buffer != nullptr) {
        UnmapViewOfFile(this->m_buffer);
        CloseHandle(this->m_mapping);
        CloseHandle(this->m_file);

        this->m_buffer = nullptr;
        this->m_file = nullptr;
        this->m_mapping = nullptr;
        this->m_size = 0;
    }
#else
    if (this->m_buffer != nullptr) {
        if (munmap(this->m_buffer, this->m_size) == -1) {
            LERRORC(LOGGER_CAT, "File unmapping failed!");
            std::terminate();
        }
        close(this->m_fd);

        this->m_buffer = nullptr;
        this->m_size = 0;
        this->m_fd = -1;
    }
#endif // _WIN32
}

ReadWriteMappedFile& ReadWriteMappedFile::operator=(ReadWriteMappedFile&& other) noexcept {
    using std::swap;

    if (this != &other) {
        swap(*this, other);
    }

    return *this;
}

void ReadWriteMappedFile::flush() const {
#ifdef _WIN32
    if (!FlushViewOfFile(this->m_buffer, this->m_size)) {
        LERRORC(LOGGER_CAT, "Flush failed!");
        throw VoreenException("ReadWriteMappedFile: Flush failed!");
    }
#else
    if (msync(this->m_buffer, this->m_size, MS_SYNC) == -1) {
        LERRORC(LOGGER_CAT, "Flush failed!");
        throw VoreenException("ReadWriteMappedFile: Flush failed!");
    }
#endif // _WIN32
}


std::size_t ReadWriteMappedFile::ensure_capacity_inner(const std::size_t cursor, const std::size_t size,
                                                       const std::size_t alignment) {
    if (cursor > this->m_size) {
        LERRORC(LOGGER_CAT, "Cursor out of bounds!");
        throw VoreenException("ReadWriteMappedFile: Cursor out of bounds!");
    }

    const std::size_t start = (cursor + alignment - 1) & ~(alignment - 1);
    const std::size_t end = start + size;
    if (end <= this->m_size) {
        return start;
    }

#ifdef _WIN32
    UnmapViewOfFile(this->m_buffer);
    CloseHandle(this->m_mapping);

    this->m_buffer = nullptr;
    this->m_mapping = nullptr;
    this->m_size = end;

    LARGE_INTEGER li;
    li.QuadPart = end;

    if (SetFilePointer(file, li.LowPart, &li.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(this->m_file);
        this->m_file = nullptr;
        this->m_size = 0;
        LERRORC(LOGGER_CAT, "Failed to move the file pointer to the end of the file!");
        throw VoreenException("ReadWriteMappedFile: Failed to move the file pointer to the end of the file!");
    }

    if (!SetEndOfFile(this->m_file)) {
        CloseHandle(this->m_file);
        this->m_file = nullptr;
        this->m_size = 0;
        LERRORC(LOGGER_CAT, "Failed to extend file!");
        throw VoreenException("ReadWriteMappedFile: Failed to extend file!");
    }

    if (SetFilePointer(this->m_file, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(this->m_file);
        this->m_file = nullptr;
        this->m_size = 0;
        LERRORC(LOGGER_CAT, "Failed to move the file pointer to the start of the file!");
        throw VoreenException("ReadWriteMappedFile: Failed to move the file pointer to the start of the file!");
    }

    HANDLE mapping = CreateFileMappingA(this->m_file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (mapping == nullptr) {
        CloseHandle(this->m_file);
        this->m_file = nullptr;
        this->m_size = 0;
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: Failed to create file mapping!");
    }

    LPVOID addr = MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (addr == nullptr) {
        CloseHandle(mapping);
        CloseHandle(this->m_file);
        this->m_file = nullptr;
        this->m_size = 0;
        LERRORC(BINARY_FILE_LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: File mapping failed!");
    }

    this->m_buffer = addr;
    this->m_mapping = mapping;
#else
    constexpr off_t MAX_LENGTH = std::numeric_limits<off_t>::max();
    if (end > MAX_LENGTH) {
        LERRORC(LOGGER_CAT, "Maximum file length reached!");
        throw VoreenException("ReadWriteMappedFile: Maximum file length reached!");
    }

    if (ftruncate(this->m_fd, static_cast<off_t>(end)) == -1) {
        LERRORC(LOGGER_CAT, "Failed to extend file!");
        throw VoreenException("ReadWriteMappedFile: Failed to extend file!");
    }

    if (munmap(this->m_buffer, this->m_size) == -1) {
        LERRORC(LOGGER_CAT, "File unmapping failed!");
        throw VoreenException("ReadWriteMappedFile: File unmapping failed!");
    }

    this->m_size = end;
    this->m_buffer = mmap(nullptr, end, PROT_READ | PROT_WRITE, MAP_SHARED, this->m_fd, 0);
    if (this->m_buffer == MAP_FAILED) {
        close(this->m_fd);
        this->m_size = 0;
        this->m_fd = -1;
        LERRORC(LOGGER_CAT, "File mapping failed!");
        throw VoreenException("ReadWriteMappedFile: File mapping failed!");
    }

    return start;
#endif // _WIN32
}


}
