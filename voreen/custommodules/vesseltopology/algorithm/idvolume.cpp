/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2015 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
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

#include "idvolume.h"
#include "tgt/filesystem.h"
#include "custommodules/bigdataimageprocessing/volumefiltering/slicereader.h"
#include "voreen/core/voreenapplication.h"
#include "modules/core/io/rawvolumereader.h"
#include "../util/tasktimelogger.h"

#include "tgt/memory.h"

namespace voreen {

static uint64_t toLinearPos(const tgt::svec3& pos, const tgt::svec3& dimensions) {
    return pos.x + dimensions.x*(pos.y + dimensions.y*pos.z);
}
static tgt::svec3 fromLinearPos(uint64_t pos, const tgt::svec3& dimensions) {
    tgt::svec3 p;
    p.x = pos % dimensions.x;
    pos /= dimensions.x;
    p.y = pos % dimensions.y;
    pos /= dimensions.y;
    p.z = pos;
    return p;
}

/// IdVolumeStorageInitializer -----------------------------------------------
IdVolumeStorageInitializer::IdVolumeStorageInitializer(std::string filename)
    : file_(filename, std::ofstream::binary | std::ofstream::trunc)
    , filename_(filename)
{
}

IdVolumeStorageInitializer::IdVolumeStorageInitializer(IdVolumeStorageInitializer&& other)
    : file_(std::move(other.file_))
    , filename_(other.filename_)
{
}

IdVolumeStorageInitializer::~IdVolumeStorageInitializer()
{
}

void IdVolumeStorageInitializer::push(IdVolume::Value val) {
    file_.write(reinterpret_cast<char*>(&val), sizeof(val));
}

/// IdVolumeStorage ----------------------------------------------------------
IdVolumeStorage::IdVolumeStorage(IdVolumeStorageInitializer&& initializer, tgt::svec3 dimensions)
    : file_()
    , dimensions_(dimensions)
    , filename_(initializer.filename_)
{
    size_t numVoxels = tgt::hmul(dimensions);

    boost::iostreams::mapped_file_params openParams;
    openParams.path = initializer.filename_;
    openParams.mode = std::ios::in | std::ios::out;
    openParams.length = numVoxels*sizeof(IdVolume::Value);

    {
        //Destroy initializer and thus close the file
        auto _dump = std::move(initializer);
    }

    file_.open(openParams);
}
IdVolumeStorage::~IdVolumeStorage() {
    file_.close();
    if(!filename_.empty()) {
        tgt::FileSystem::deleteFile(filename_);
    }
}
void IdVolumeStorage::set(const tgt::svec3& pos, IdVolume::Value val) {
    tgtAssert(file_.is_open(), "File is not open");
    tgtAssert(pos.x < dimensions_.x && pos.y < dimensions_.y && pos.z < dimensions_.z, "Invalid pos");

    size_t index = toLinearPos(pos, dimensions_);

    reinterpret_cast<IdVolume::Value*>(file_.data())[index] = val;
}
IdVolume::Value IdVolumeStorage::get(const tgt::svec3& pos) const {
    tgtAssert(file_.is_open(), "File is not open");
    tgtAssert(pos.x < dimensions_.x && pos.y < dimensions_.y && pos.z < dimensions_.z, "Invalid pos");

    size_t index = toLinearPos(pos, dimensions_);

    return reinterpret_cast<IdVolume::Value*>(file_.data())[index];
}

/// IdVolume -------------------------------------------------------------------
const IdVolume::Value IdVolume::BACKGROUND_VALUE = 0xffffffff;
const IdVolume::Value IdVolume::UNLABELED_FOREGROUND_VALUE = 0xfffffffe;

IdVolume::IdVolume(IdVolume&& other)
    : data_(std::move(other.data_))
    , surfaceFile_(other.surfaceFile_)
    , numUnlabeledForegroundVoxels_(other.numUnlabeledForegroundVoxels_)
{
    other.data_ = nullptr;
    other.surfaceFile_ = StoredSurface("",0);
}

IdVolume::IdVolume(IdVolumeStorageInitializer&& storage, StoredSurface surface, tgt::svec3 dimensions, size_t numUnlabeledForegroundVoxels)
    : data_(tgt::make_unique<IdVolumeStorage>(std::move(storage), dimensions))
    , surfaceFile_(surface)
    , numUnlabeledForegroundVoxels_(numUnlabeledForegroundVoxels)
{
}

IdVolume::~IdVolume() {
    tgt::FileSystem::deleteFile(surfaceFile_.filename_);
}

/*
IdVolume::IdVolume(IdVolumeInitializationReader& initializationReader)
    : data_(nullptr)
    , surfaceFile_("", 0)
    , numUnlabeledForegroundVoxels_(0)
{
    //TaskTimeLogger _("Build IdVolume", tgt::Info);
    const tgt::ivec3 dim = initializationReader.getDimensions();

    SurfaceBuilder builder;
    IdVolumeStorageInitializer initializer(VoreenApplication::app()->getUniqueTmpFilePath(".raw"));

    for(size_t z=0; z < dim.z; ++z) {
        initializationReader.advance();

        for(size_t y=0; y < dim.y; ++y) {
            for(size_t x=0; x < dim.x; ++x) {
                tgt::svec3 pos(x,y,z);
                uint32_t label;
                if(initializationReader.isObject(pos)) {
                    if(initializationReader.isLabeled(pos)) {
                        label = initializationReader.getBranchId(pos);

                        bool isLabelSurfaceVoxel =
                            !( (x == 0       || initializationReader.isLabeled(tgt::ivec3(x-1, y  , z  )))
                            && (x == dim.x-1 || initializationReader.isLabeled(tgt::ivec3(x+1, y  , z  )))
                            && (y == 0       || initializationReader.isLabeled(tgt::ivec3(x  , y-1, z  )))
                            && (y == dim.y-1 || initializationReader.isLabeled(tgt::ivec3(x  , y+1, z  )))
                            && (z == 0       || initializationReader.isLabeled(tgt::ivec3(x  , y  , z-1)))
                            && (z == dim.z-1 || initializationReader.isLabeled(tgt::ivec3(x  , y  , z+1))));

                        if(isLabelSurfaceVoxel) {
                            builder.push(toLinearPos(tgt::svec3(pos), dim));
                        }
                    } else {
                        label = IdVolume::UNLABELED_FOREGROUND_VALUE;
                        ++numUnlabeledForegroundVoxels_;
                    }
                } else {
                    label = IdVolume::BACKGROUND_VALUE;
                }
                initializer.push(label);
            }
        }
    }

    data_.reset(new IdVolumeStorage(std::move(initializer), dim));
    surfaceFile_ = SurfaceBuilder::finalize(std::move(builder));
}
*/

void IdVolume::floodIteration(size_t& numberOfFloodedVoxels, ProgressReporter& progress) {
    SurfaceReader surfaceReader(surfaceFile_);
    SurfaceBuilder builder;

    SurfaceSlices<3> surface;

    size_t z = 0;
    size_t currentVoxel = 0;
    size_t updateInterval = 10000;
    tgt::svec3 dimensions = data_->dimensions_;
    for(uint64_t linearPos = -1; surfaceReader.read(linearPos); ++currentVoxel) {
        tgtAssert(linearPos < tgt::hmul(dimensions), "Invalid linear pos read from file");
        if((currentVoxel % updateInterval) == 0) {
            progress.setProgress(static_cast<float>(numberOfFloodedVoxels)/numUnlabeledForegroundVoxels_);
        }

        tgt::svec3 pos = fromLinearPos(linearPos, dimensions);

        IdVolume::Value current_label = data_->get(pos);
        tgtAssert(current_label != UNLABELED_FOREGROUND_VALUE && current_label != BACKGROUND_VALUE, "invalid surface label");

        while(pos.z != z) {
            tgtAssert(pos.z > z, "pos too small");

            surface.advance(builder);
            ++z;
        }
        auto label_if_in_volume = [&] (std::set<uint64_t>& set, tgt::ivec3 offset) {
            tgt::ivec3 npos = pos;
            npos += offset;
            if(
                       npos.x >= 0 && npos.x < dimensions.x
                    && npos.y >= 0 && npos.y < dimensions.y
                    && npos.z >= 0 && npos.z < dimensions.z
              ) {
                if(data_->get(npos) == UNLABELED_FOREGROUND_VALUE) {
                    data_->set(npos, current_label);
                    set.insert(toLinearPos(tgt::svec3(npos), dimensions));
                    ++numberOfFloodedVoxels;
                }
            }
        };

        label_if_in_volume(surface.m<0>(), tgt::ivec3( 0, 0, 1));
        label_if_in_volume(surface.m<1>(), tgt::ivec3( 1, 0, 0));
        label_if_in_volume(surface.m<1>(), tgt::ivec3(-1, 0, 0));
        label_if_in_volume(surface.m<1>(), tgt::ivec3( 0, 1, 0));
        label_if_in_volume(surface.m<1>(), tgt::ivec3( 0,-1, 0));
        label_if_in_volume(surface.m<2>(), tgt::ivec3( 0, 0,-1));

    }
    for(int i=0; i<3; ++i) {
        surface.advance(builder);
    }
    tgtAssert(surface.m<0>().empty() && surface.m<1>().empty() && surface.m<2>().empty(), "Writing surface back unfinished");

    surfaceFile_ = SurfaceBuilder::finalize(std::move(builder));

    progress.setProgress(static_cast<float>(numberOfFloodedVoxels)/numUnlabeledForegroundVoxels_);
}

void IdVolume::floodFromLabels(ProgressReporter& progress, size_t maxIt) {
    //TaskTimeLogger _("Flood labels", tgt::Info);
    size_t flooded_this_it = 0;
    size_t flooded_prev_it = -1;
    size_t it = 0;
    while(flooded_this_it != flooded_prev_it && it < maxIt) {
        flooded_prev_it = flooded_this_it;
        floodIteration(flooded_this_it, progress);
        size_t flood_progress = flooded_this_it - flooded_prev_it;
        //LINFO("Flood iteration " << it << " marked " << flood_progress << " voxels.");
        ++it;
    }
    //LINFO("Flooding finished and marked " << flooded_this_it << " voxels.");
}

tgt::svec3 IdVolume::getDimensions() const {
    return data_->dimensions_;
}

const std::string IdVolume::loggerCat_ = "vesseltopology.idvolume";

}
