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

#ifndef VRN_MA_UTILITY_H
#define VRN_MA_UTILITY_H

#include <cstddef>
#include <cstdint>
#include <cassert>

#include "memorymappedfile.h"

namespace voreen {

template <typename T>
struct MAPointer {
    using Element = T;
    constexpr static MAPointer INVALID = static_cast<std::uint64_t>(-1);

    std::uint64_t cursor;

    void assert_valid() const {
        assert(this->cursor != INVALID);
    }

    const Element* deref(const ReadOnlyMappedFile& f) const {
        this->assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->cursor));
    }

    const Element* deref(const ReadWriteMappedFile& f) const {
        this->assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->cursor));
    }

    Element* deref(ReadWriteMappedFile& f) const {
        this->assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->cursor));
    }
};

template <typename T>
struct MASpan {
    using Element = T;

    std::uint64_t size = 0;
    MAPointer<T> element = MAPointer<T>::INVALID;

    MappedRange<const Element> deref(const ReadOnlyMappedFile& f) const {
        this->element.assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->element.cursor),
                             static_cast<std::size_t>(this->size));
    }

    MappedRange<const Element> deref(const ReadWriteMappedFile& f) const {
        this->element.assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->element.cursor),
                             static_cast<std::size_t>(this->size));
    }

    MappedRange<Element> deref(ReadWriteMappedFile& f) const {
        this->element.assert_valid();
        return f.at<Element>(static_cast<std::size_t>(this->element.cursor),
                             static_cast<std::size_t>(this->size));
    }
};

template <typename T>
struct MALinkedList {
    using Element = T;

    T element;
    MAPointer<MALinkedList> next = MAPointer<MALinkedList>::INVALID;
};

struct MAIndex {
    std::uint64_t x;
    std::uint64_t y;
    std::uint64_t z;
    std::uint64_t u;
    std::uint64_t v;
};

struct MAExtent {
    std::uint64_t x;
    std::uint64_t y;
    std::uint64_t z;
    std::uint64_t u;
    std::uint64_t v;
};

struct MAWeight {
    std::uint64_t a;
    std::uint64_t b;
};

enum class MADataTag: std::uint8_t {
    UnsignedInt,
    SignedInt,
    Float,
    String,
};

struct MAData {
    MADataTag tag = MADataTag::UnsignedInt;
    const char* key = "";

    union {
        MASpan<std::uint64_t> unsigned_int = {};
        MASpan<std::int64_t> signed_int;
        MASpan<float> floating_point;
        MASpan<MAPointer<float>> string;
    } value;
};

} // namespace voreen

#endif // VRN_MA_UTILITY_H
