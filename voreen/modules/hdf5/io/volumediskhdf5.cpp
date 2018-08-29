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

#include "volumediskhdf5.h"

#include "voreen/core/utils/hashing.h"


namespace voreen {

const std::string VolumeDiskHDF5::loggerCat_("voreen.hdf5.VolumeDiskHDF5");


VolumeDiskHDF5::VolumeDiskHDF5(std::unique_ptr<HDF5FileVolume> volume, size_t channel)
    : VolumeDisk(volume->getBaseType(), volume->getDimensions())
    , volume_(std::move(volume))
    , channel_(channel)
{
}

VolumeDiskHDF5::~VolumeDiskHDF5() {
}

std::string VolumeDiskHDF5::getHash() const {
    std::string configStr;

    configStr += volume_->getFileName() + "#";
    configStr += volume_->getVolumeLocation() + "#";
    configStr += std::to_string(channel_) + "#";

    return VoreenHash::getHash(configStr);
}

VolumeRAM* VolumeDiskHDF5::loadVolume() const {
    return volume_->loadBrick(tgt::vec3(0,0,0), dimensions_, channel_);
}

VolumeRAM* VolumeDiskHDF5::loadSlices(const size_t firstZSlice, const size_t lastZSlice) const {
    return volume_->loadSlices(firstZSlice, lastZSlice, channel_);
}

VolumeRAM* VolumeDiskHDF5::loadBrick(const tgt::svec3& offset, const tgt::svec3& dimensions) const {
    return volume_->loadBrick(offset, dimensions, channel_);
}

} // namespace voreen
