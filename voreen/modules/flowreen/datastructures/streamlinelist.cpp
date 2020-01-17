/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2020 University of Muenster, Germany,                        *
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

#include "streamlinelist.h"

#include "voreen/core/datastructures/volume/volume.h"
#include "voreen/core/utils/stringutils.h"

#include "tgt/assert.h"

#include <sstream>

namespace voreen {

StreamlineList::StreamlineList(const VolumeBase* vol)
    : StreamlineListBase()
    , dimensions_(1), spacing_(1.f), minMagnitude_(-1.f), maxMagnitude_(-1.f)
    , listTransformMatrix_(tgt::mat4::identity), velocityTransformMatrix_(tgt::mat4::identity)
{
    if(vol) {
        dimensions_ = vol->getDimensions();
        spacing_ = vol->getSpacing();
        worldBounds_ = vol->getBoundingBox().getBoundingBox();
        voxelToWorldMatrix_ = vol->getVoxelToWorldMatrix();
        worldToVoxelMatrix_ = vol->getWorldToVoxelMatrix();
        voxelBounds_.addPoint(worldToVoxelMatrix_ * worldBounds_.getLLF());
        voxelBounds_.addPoint(worldToVoxelMatrix_ * worldBounds_.getURB());
    }
}

StreamlineList::~StreamlineList() {
}

StreamlineListBase* StreamlineList::clone() const{
    StreamlineList* result = new StreamlineList();

    result->streamlines_             = this->streamlines_;
    result->dimensions_              = this->dimensions_;
    result->spacing_                 = this->spacing_;
    result->worldBounds_             = this->worldBounds_;
    result->voxelBounds_             = this->voxelBounds_;
    result->voxelToWorldMatrix_      = this->voxelToWorldMatrix_;
    result->worldToVoxelMatrix_      = this->worldToVoxelMatrix_;
    result->minMagnitude_            = this->minMagnitude_;
    result->maxMagnitude_            = this->maxMagnitude_;
    result->listTransformMatrix_     = this->listTransformMatrix_;
    result->velocityTransformMatrix_ = this->velocityTransformMatrix_;

    return result;
}

    //------------------------
    //  Streamline Handling
    //------------------------
void StreamlineList::addStreamline(const Streamline& line) {
    notifyPendingDataInvalidation();

    streamlines_.push_back(line);

    //ignore degenerated lines
    if(line.getNumElements() < 2) return;

    if(minMagnitude_ < 0.f || minMagnitude_ > line.getMinMagnitude()) {
        minMagnitude_ = line.getMinMagnitude();
    }
    if(maxMagnitude_ < 0.f || maxMagnitude_ < line.getMaxMagnitude()) {
        maxMagnitude_ = line.getMaxMagnitude();
    }
}

void StreamlineList::addStreamlineList(const StreamlineListBase& list) {
    //return if list is empty
    if(list.getStreamlines().empty()) return;

    notifyPendingDataInvalidation();

    //adapt min and max value
    if(minMagnitude_ < 0.f || minMagnitude_ > list.getMinMagnitude()) {
        minMagnitude_ = list.getMinMagnitude();
    }
    if(maxMagnitude_ < 0.f || maxMagnitude_ < list.getMaxMagnitude()) {
        maxMagnitude_ = list.getMaxMagnitude();
    }

    //copy streamlines
    streamlines_.insert(streamlines_.end(), list.getStreamlines().begin(), list.getStreamlines().end());
}

void StreamlineList::removeStreamline(size_t pos) {
    tgtAssert(pos < streamlines_.size(), "Index out of bounds.");
    notifyPendingDataInvalidation();
    streamlines_[pos] = streamlines_[streamlines_.size()-1];
    streamlines_.pop_back();
}

void StreamlineList::clearStreamlines() {
    notifyPendingDataInvalidation();
    streamlines_.clear();
}

const std::vector<Streamline>& StreamlineList::getStreamlines() const {
    return streamlines_;
}

    //----------------
    //  Meta
    //----------------
const tgt::svec3& StreamlineList::getOriginalDimensions() const {
    return dimensions_;
}

const tgt::vec3& StreamlineList::getOriginalSpacing() const {
    return spacing_;
}

const tgt::Bounds& StreamlineList::getOriginalVoxelBounds() const {
    return voxelBounds_;
}

const tgt::Bounds& StreamlineList::getOriginalWorldBounds() const {
    return worldBounds_;
}

const tgt::mat4& StreamlineList::getOriginalVoxelToWorldMatrix() const {
    return voxelToWorldMatrix_;
}

const tgt::mat4& StreamlineList::getOriginalWorldToVoxelMatrix() const {
    return worldToVoxelMatrix_;
}

float StreamlineList::getMinMagnitude() const {
    return std::max(0.f,minMagnitude_);
}

float StreamlineList::getMaxMagnitude() const {
    return std::max(0.f,maxMagnitude_);
}

const tgt::mat4& StreamlineList::getListTransformMatrix() const {
    return listTransformMatrix_;
}

const tgt::mat4& StreamlineList::getVelocityTransformMatrix() const {
    return velocityTransformMatrix_;
}

const tgt::mat4 StreamlineList::getVoxelToWorldMatrix() const {
    return listTransformMatrix_ * voxelToWorldMatrix_;
}

void StreamlineList::setTransformMatrices(const tgt::mat4& listMatrix, const tgt::mat4& velocityMatrix) {
    notifyPendingDataInvalidation();
    listTransformMatrix_ = listMatrix;
    velocityTransformMatrix_ = velocityMatrix;
}

    //----------------
    //  Storage
    //----------------
std::string StreamlineList::metaToCSVString() const {
    std::stringstream output;
    output << streamlines_.size() << std::endl;
    output << minMagnitude_ << ", " << maxMagnitude_ << std::endl;
    output << dimensions_.x << ", " << dimensions_.y << ", " << dimensions_.z << std::endl;
    output << spacing_.x << ", " << spacing_.y << ", " << spacing_.z << std::endl;
    output << worldBounds_.getLLF().x << ", " << worldBounds_.getLLF().y << ", " << worldBounds_.getLLF().z << std::endl;
    output << worldBounds_.getURB().x << ", " << worldBounds_.getURB().y << ", " << worldBounds_.getURB().z << std::endl;
    return output.str();
}

void StreamlineList::serialize(Serializer& s) const {
    tgt::ivec3 tmpDim = dimensions_;
    s.serialize("OriginalDimensions",tmpDim);
    s.serialize("OriginalSpacing", spacing_);
    s.serialize("OriginalWorldBounds",worldBounds_);
    s.serialize("OriginalVoxelBounds",voxelBounds_);
    s.serialize("VoxelToWorld",voxelToWorldMatrix_);
    s.serialize("WorldToVoxel",worldToVoxelMatrix_);
    s.serialize("minMagnitude_",minMagnitude_);
    s.serialize("maxMagnitude_",maxMagnitude_);
    s.serialize("listTransformMatrix",listTransformMatrix_);
    s.serialize("velocityTransformMatrix",velocityTransformMatrix_);

    // Currently, we do only support up to 2 ^ 16 Streamlines
    // which is being limited by the user interface.
    // Hence, we serialize and amounts as integers to save memory.

    s.serialize("NumStreamlines", static_cast<int>(streamlines_.size()));
    for(size_t i = 0; i < streamlines_.size(); i++) {
        s.serialize("Streamline" + itos(i,5), streamlines_[i]);
    }
}

void StreamlineList::deserialize(Deserializer& s) {
    tgt::ivec3 tmpDim = dimensions_;
    s.deserialize("OriginalDimensions",tmpDim);
    dimensions_ = tmpDim;
    s.deserialize("OriginalSpacing", spacing_);
    s.deserialize("OriginalWorldBounds",worldBounds_);
    s.deserialize("OriginalVoxelBounds",voxelBounds_);
    s.deserialize("VoxelToWorld",voxelToWorldMatrix_);
    s.deserialize("WorldToVoxel",worldToVoxelMatrix_);
    s.deserialize("minMagnitude_",minMagnitude_);
    s.deserialize("maxMagnitude_",maxMagnitude_);
    s.optionalDeserialize("listTransformMatrix",listTransformMatrix_, tgt::mat4::identity);
    s.optionalDeserialize("velocityTransformMatrix",velocityTransformMatrix_, tgt::mat4::identity);

    size_t numStreamlines = 0;
    s.deserialize("NumStreamlines", numStreamlines);
    streamlines_.clear();
    streamlines_.resize(numStreamlines);
    for(size_t i = 0; i < numStreamlines; i++) {
        s.deserialize("Streamline" + itos(i,5), streamlines_[i]);
    }
}

}   // namespace
