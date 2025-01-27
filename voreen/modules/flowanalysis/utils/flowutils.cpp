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

#include "flowutils.h"

namespace voreen {

SpatialSampler::SpatialSampler(const VolumeRAM* volume,
                               const RealWorldMapping& rwm,
                               VolumeRAM::Filter filter,
                               const tgt::mat4& toVoxelMatrix,
                               const tgt::mat4& velocityTransformationMatrix)
    : toVoxelMatrix_(toVoxelMatrix)
    , toVoxelMatrixSet_(toVoxelMatrix_ != tgt::mat4::identity)
    , velocityTransformationMatrix_(velocityTransformationMatrix)
    , velocityTransformationMatrixSet_(velocityTransformationMatrix != tgt::mat4::identity)
{
    switch(filter) {
    case VolumeRAM::NEAREST:
        sampleFunction_ = [volume, rwm] (const tgt::vec3& pos) {
            tgt::vec3 voxel = tgt::vec3::zero;
            for (size_t channel = 0; channel < volume->getNumChannels(); channel++) {
                voxel[channel] = rwm.normalizedToRealWorld(volume->getVoxelNormalized(pos, channel));
            }
            return voxel;
        };
        break;
    case VolumeRAM::LINEAR:
        sampleFunction_ = [volume, rwm] (const tgt::vec3& pos) {
            tgt::vec3 voxel = tgt::vec3::zero;
            for (size_t channel = 0; channel < volume->getNumChannels(); channel++) {
                voxel[channel] = rwm.normalizedToRealWorld(volume->getVoxelNormalizedLinear(pos, channel));
            }
            return voxel;
        };
            break;
    case VolumeRAM::CUBIC:
        sampleFunction_ = [volume, rwm] (const tgt::vec3& pos) {
            tgt::vec3 voxel = tgt::vec3::zero;
            for (size_t channel = 0; channel < volume->getNumChannels(); channel++) {
                voxel[channel] = rwm.normalizedToRealWorld(volume->getVoxelNormalizedCubic(pos, channel));
            }
            return voxel;
        };
            break;
    default:
        tgtAssert(false, "unhandled filter mode");
    }
}

tgt::vec3 SpatialSampler::sample(tgt::vec3 pos) const {
    if(toVoxelMatrixSet_) {
        pos = toVoxelMatrix_ * pos;
    }

    auto velocity = sampleFunction_(pos);

    if(velocityTransformationMatrixSet_) {
        velocity = velocityTransformationMatrix_ * velocity;
    }

    return velocity;
}


SpatioTemporalSampler::SpatioTemporalSampler(const VolumeRAM* volume0,
                                             const VolumeRAM* volume1,
                                             float alpha,
                                             const RealWorldMapping& rwm,
                                             VolumeRAM::Filter filter,
                                             const tgt::mat4& toVoxelMatrix,
                                             const tgt::mat4& velocityTransformationMatrix)
    : filter0_(volume0, rwm, filter, toVoxelMatrix, velocityTransformationMatrix)
    , filter1_(volume1, rwm, filter, toVoxelMatrix, velocityTransformationMatrix)
    , alpha_(alpha)
{
    tgtAssert(alpha_ >= 0.0f && alpha_ <= 1.0f, "Alpha must be in range [0, 1]");
}

tgt::vec3 SpatioTemporalSampler::sample(tgt::vec3 pos) const {
    tgt::vec3 voxel0 = filter0_.sample(pos);
    tgt::vec3 voxel1 = filter1_.sample(pos);
    return voxel0 * (1.0f - alpha_) + voxel1 * alpha_;
}


tgt::mat4 createTransformationMatrix(const tgt::vec3& position, const tgt::vec3& velocity) {

    tgt::vec3 tangent(tgt::normalize(velocity));

    tgt::vec3 temp(0.0f, 0.0f, 1.0f);
    if(1.0f - std::abs(tgt::dot(temp, tangent)) <= std::numeric_limits<float>::epsilon())
        temp = tgt::vec3(0.0f, 1.0f, 0.0f);

    tgt::vec3 binormal(tgt::normalize(tgt::cross(temp, tangent)));
    tgt::vec3 normal(tgt::normalize(tgt::cross(tangent, binormal)));

    return tgt::mat4(normal.x, binormal.x, tangent.x, position.x,
                     normal.y, binormal.y, tangent.y, position.y,
                     normal.z, binormal.z, tangent.z, position.z,
                     0.0f, 0.0f, 0.0f, 1.0f);
}

}