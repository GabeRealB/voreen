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

#ifndef VRN_MA_FIELD_H
#define VRN_MA_FIELD_H

#include <cstdint>

#include "utility.h"

namespace voreen {

struct MAFieldStage {
    MAExtent input_extent = {};
    MAExtent output_extent = {};
    MAExtent chunks_count = {};
    MAExtent steps_count = {};
    MASpan<float> chunks = {};
    MASpan<float> weights = {};
};

struct MAField {
    std::uint8_t magic[7] = {'m', 'a', 'f', 'i', 'e', 'l', 'd'};
    std::uint8_t version = VERSION;
    std::uint32_t flags = static_cast<std::uint32_t>(Flag::None);
    std::uint32_t components_per_element = 0;
    MAExtent extent = {};
    MAExtent steps = {};
    MALinkedList<MAData> embedded_data = {};
    MALinkedList<MAFieldStage> stages = {};
    MASpan<float> leaf = {};
    MAPointer<void> reserved = MAPointer<void>::INVALID;

    constexpr static std::uint8_t MAGIC[7] = {'m', 'a', 'f', 'i', 'e', 'l', 'd'};
    constexpr static std::uint8_t VERSION = 0;

    enum class Flag : std::uint32_t {
        None = 0,
        Weighted = 1,
    };
};

} // namespace voreen

#endif // VRN_MA_FIELD_H
