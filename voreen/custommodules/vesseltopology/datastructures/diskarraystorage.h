/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany,                        *
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
#pragma once
#include <string>
#include <vector>
#include <boost/iostreams/device/mapped_file.hpp>
#include "tgt/filesystem.h"

template<typename Element>
class DiskArray {
public:
    DiskArray(const boost::iostreams::mapped_file_source& file_, size_t begin, size_t end);
    ~DiskArray() {}

    const Element& operator[](size_t index) const;
    size_t size() const;
private:
    const boost::iostreams::mapped_file_source& file_;
    size_t begin_;
    size_t end_;
};

template<typename Element>
class DiskArrayStorage {
public:
    DiskArrayStorage(const std::string& storagefilename);
    ~DiskArrayStorage();

    // Note: The Store must live longer than the returned DiskArray!
    // Absolutely not threadsafe!
    DiskArray<Element> store(const std::vector<Element>& elements);

private:
    void ensureFit(size_t numElements);

    boost::iostreams::mapped_file file_;
    size_t numElements_;
    const std::string storagefilename_;
    size_t physicalFileSize_;
};


/// Impl: DiskArray ------------------------------------------------------------
template<typename Element>
DiskArray<Element>::DiskArray(const boost::iostreams::mapped_file_source& file, size_t begin, size_t end)
    : file_(file)
    , begin_(begin)
    , end_(end)
{
}

template<typename Element>
const Element& DiskArray<Element>::operator[](size_t index) const {
    size_t fileIndex = index + begin_;
    tgtAssert(fileIndex < end_, "Invalid index");
    const Element* data = reinterpret_cast<const Element*>(file_.data());
    return data[fileIndex];
}

template<typename Element>
size_t DiskArray<Element>::size() const {
    return end_ - begin_;
}

/// Impl: DiskArrayStorage -----------------------------------------------------
template<typename Element>
DiskArrayStorage<Element>::DiskArrayStorage(const std::string& storagefilename)
    : file_()
    , numElements_(0)
    , storagefilename_(storagefilename)
    , physicalFileSize_(1024)
{
    ensureFit(numElements_);
}

template<typename Element>
DiskArrayStorage<Element>::~DiskArrayStorage() {
    file_.close();
    tgt::FileSystem::deleteFile(storagefilename_);
}

static void allocateFile(const std::string& fileName, size_t size) {
    std::ofstream file(fileName);
    file.seekp(size);
    file << '\0';
}
template<typename Element>
void DiskArrayStorage<Element>::ensureFit(size_t numElements) {
    size_t requiredFileSize = numElements * sizeof(Element);
    if(requiredFileSize > physicalFileSize_) {
        file_.close();

        physicalFileSize_ *= 2;
        allocateFile(storagefilename_, physicalFileSize_);

        boost::iostreams::mapped_file_params openParams;
        openParams.path = storagefilename_;
        openParams.mode = std::ios::in | std::ios::out;

        file_.open(openParams);
        tgtAssert(file_.is_open(), "File not open");
    }
}

template<typename Element>
DiskArray<Element> DiskArrayStorage<Element>::store(const std::vector<Element>& elements) {
    size_t oldNumElements = numElements_;
    numElements_ += elements.size();

    ensureFit(numElements_);

    return DiskArray<Element>(file_, oldNumElements, numElements_);
}
