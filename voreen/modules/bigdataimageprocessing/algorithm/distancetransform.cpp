/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2021 University of Muenster, Germany,                        *
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

#include "distancetransform.h"

#include <vector>

#include "tgt/vector.h"

namespace voreen {

template<typename T>
static inline T square(T i) {
    return i*i;
}

template<int outerDim, int innerDim, typename InputSlice, typename OutputSlice, typename InitValFunc, typename FinalValFunc>
static void dt_slice_pass(InputSlice& inputSlice, OutputSlice& outputSlice, tgt::ivec3 dim, tgt::vec3 spacingVec, InitValFunc initValFunc, FinalValFunc finalValFunc) {
    const int n = dim[innerDim];
    std::vector<int> v(n+1, 0); // Locations of parabolas in lower envelope, voxel coordinates
    std::vector<float> z(n+1, 0); // Locations of boundaries between parabolas, physical coordinates (i.e., including spacing)

    float spacing = spacingVec[innerDim];
    tgtAssert(spacing > 0, "Invalid spacing");

    for(int x = 0; x < dim[outerDim]; ++x) {
        auto f = [&] (int i) {
            tgt::svec3 slicePos(i,i,0);
            slicePos[outerDim] = x;
            float g = inputSlice->voxel(slicePos);
            return initValFunc(g);
        };

        v[0] = 0;
        z[0] = -std::numeric_limits<float>::infinity();
        z[1] = std::numeric_limits<float>::infinity();
        int k = 0;

        for(int q = 1; q < n; ++q) {
            float fq = f(q);
            if(std::isinf(fq)) {
                continue;
            }
jmp:
            int vk = v[k];
            float qs = spacing * q;
            float vks = spacing * vk;
            float s = ((fq - f(vk)) + (square(qs) - square(vks))) / (2*(qs - vks)); //note: q > vk
            tgtAssert(!std::isnan(s), "s is nan");

            if(s <= z[k]) {
                if(k > 0) {
                    k -= 1;
                    goto jmp;
                } else {
                    v[k] = q;
                    z[k] = s;
                    z[k+1] = std::numeric_limits<float>::infinity();
                }
            } else {
                k += 1;
                v[k] = q;
                z[k] = s;
                z[k+1] = std::numeric_limits<float>::infinity();
            }
        }

        k = 0;
        for(int q = 0; q < n; ++q) {
            float qs = spacing * q;
            while(z[k+1] < qs) {
                k += 1;
            }
            tgt::svec3 slicePos(q,q,0);
            slicePos[outerDim] = x;
            int vk = v[k];
            float vks = spacing * vk;
            float val = f(vk) + square(qs-vks);
            outputSlice->voxel(slicePos) = finalValFunc(val);
        }
    }
}

LZ4SliceVolume<float> compute_distance_transform(const VolumeBase& vol, float binarizationThreshold, std::string outputPath, ProgressReporter& progressReporter) {
    const tgt::svec3 dim = vol.getDimensions();
    const tgt::svec3 sliceDim(dim.x, dim.y, 1);
    const tgt::vec3 spacing = vol.getSpacing();

    LZ4SliceVolumeBuilder<float> gBuilder(outputPath,
            LZ4SliceVolumeMetadata(dim)
            .withOffset(vol.getOffset())
            .withSpacing(vol.getSpacing())
            .withPhysicalToWorldTransformation(vol.getPhysicalToWorldMatrix()));

    VolumeAtomic<float> gSlice(sliceDim);
    // z-scan 1: calculate distances in forward direction
    {
        const size_t z = 0;
        std::unique_ptr<VolumeRAM> inputSlice(vol.getSlice(z));
        for(size_t y = 0; y < dim.y; ++y) {
            for(size_t x = 0; x < dim.x; ++x) {
                tgt::svec3 slicePos(x,y,0);
                float val = inputSlice->getVoxelNormalized(slicePos);
                float& g = gSlice.voxel(slicePos);
                if(val < binarizationThreshold) {
                    // Background
                    g = 0;
                } else {
                    // Foreground
                    g = std::numeric_limits<float>::infinity();
                }
            }
        }
        gBuilder.pushSlice(gSlice);
    }
    for(size_t z = 1; z < dim.z; ++z) {
        progressReporter.setProgress(static_cast<float>(z)/dim.z);
        std::unique_ptr<VolumeRAM> inputSlice(vol.getSlice(z));


        for(size_t y = 0; y < dim.y; ++y) {
            for(size_t x = 0; x < dim.x; ++x) {
                tgt::svec3 slicePos(x,y,0);
                float val = inputSlice->getVoxelNormalized(slicePos);
                float& g = gSlice.voxel(slicePos);
                if(val < binarizationThreshold) {
                    // Background
                    g = 0;
                } else {
                    // Foreground
                    g += spacing.z;
                }
            }
        }
        gBuilder.pushSlice(gSlice);
    }
    auto gvol = std::move(gBuilder).finalize();

    auto tmpSlice = VolumeAtomic<float>(sliceDim);
    auto tmpSlicePtr = &tmpSlice;

    // z-scan 2, propagate distances in other direction
    // also, directly do y- and x- passes on the slices while they are loaded.
    {
        auto prevZSlice = gvol.loadSlice(dim.z-1);
        for(int z = dim.z-2; z >= 0; --z) {
            progressReporter.setProgress(static_cast<float>(z)/dim.z);

            auto gSlice = gvol.getWriteableSlice(z);
            for(size_t y = 0; y < dim.y; ++y) {
                for(size_t x = 0; x < dim.x; ++x) {
                    tgt::svec3 slicePos(x,y,0);
                    float& gPrev = prevZSlice.voxel(slicePos);
                    float& g = gSlice->voxel(slicePos);

                    float ng = gPrev + spacing.z;
                    if(ng < g) {
                        g = ng;
                    }
                }
            }
            prevZSlice = gSlice->copy();

            // Now do x and y passes on current slice to finalize it.
            dt_slice_pass<0,1>(gSlice, tmpSlicePtr, dim, spacing, [] (float v) {return square(v);}, [] (float v) {return v;});
            dt_slice_pass<1,0>(tmpSlicePtr, gSlice, dim, spacing, [] (float v) {return v;}, [] (float v) {return std::sqrt(v);});
        }
    }

    progressReporter.setProgress(1.f);

    return gvol;
}

}   // namespace
