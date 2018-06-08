/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany.                        *
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

#include "lz4slicevolume.h"
#include "voreen/core/voreenapplication.h"
#include "voreen/core/datastructures/volume/volume.h"
#include "tgt/memory.h"
#include "../io/volumedisklz4.h"

#include <fstream>

namespace voreen {

/// LZ4SliceVolumeMetadata -----------------------------------------------------

const static std::string METADATA_ROOT_NODE_STRING = "metadata";

LZ4SliceVolumeMetadata::LZ4SliceVolumeMetadata(tgt::svec3 dimensions)
    : dimensions_(dimensions)
{
}

void LZ4SliceVolumeMetadata::serialize(Serializer& s) const {
    s.serialize("dimensions", dimensions_);
}
void LZ4SliceVolumeMetadata::deserialize(Deserializer& s) {
    s.deserialize("dimensions", dimensions_);
}

/// LZ4SliceVolumeMetadataFull -------------------------------------------------

LZ4SliceVolumeMetadataFull::LZ4SliceVolumeMetadataFull(LZ4SliceVolumeMetadata base, std::string format, std::string baseType)
    : LZ4SliceVolumeMetadata(base)
    , format_(format)
    , baseType_(baseType)
{
}

LZ4SliceVolumeMetadataFull LZ4SliceVolumeMetadataFull::load(const std::string& xmlfile) {
    XmlDeserializer ds;
    std::ifstream filestream(xmlfile);
    ds.read(filestream);
    LZ4SliceVolumeMetadataFull metadata(LZ4SliceVolumeMetadata(tgt::svec3::zero), "", "");
    ds.deserialize(METADATA_ROOT_NODE_STRING, metadata);
    return metadata;
}

void LZ4SliceVolumeMetadataFull::save(const std::string& xmlfile) const {
    XmlSerializer ser;
    std::ofstream filestream(xmlfile);
    ser.serialize(METADATA_ROOT_NODE_STRING, *this);
}

void LZ4SliceVolumeMetadataFull::serialize(Serializer& s) const {
    LZ4SliceVolumeMetadata::serialize(s);
    s.serialize("format", format_);
    s.serialize("baseType", baseType_);
}
void LZ4SliceVolumeMetadataFull::deserialize(Deserializer& s) {
    LZ4SliceVolumeMetadata::deserialize(s);
    s.deserialize("format", format_);
    s.deserialize("baseType", baseType_);
}

/// LZ4SliceVolumeBase ---------------------------------------------------------
LZ4SliceVolumeBase::LZ4SliceVolumeBase(std::string filePath, LZ4SliceVolumeMetadataFull metadata)
    : metadata_(metadata)
    , filePath_(filePath)
{
}
const std::string LZ4SliceVolumeBase::FILE_EXTENSION = "lz4vol";

template<typename Voxel>
static void createLZ4Vol(std::string filepath, std::unique_ptr<LZ4SliceVolumeBase>& res) {
    res.reset(new LZ4SliceVolume<Voxel>(LZ4SliceVolume<Voxel>::open(filepath)));
}

std::unique_ptr<LZ4SliceVolumeBase> LZ4SliceVolumeBase::open(std::string filePath) {
    auto metadata = LZ4SliceVolumeMetadataFull::load(filePath);
    std::unique_ptr<LZ4SliceVolumeBase> res(nullptr);
    DISPATCH_FOR_FORMAT(metadata.format_, createLZ4Vol, filePath, res);
    return res;
}
std::unique_ptr<Volume> LZ4SliceVolumeBase::toVolume() && {
    //TODO
    tgt::vec3 spacing = tgt::vec3::one;
    tgt::vec3 offset = tgt::vec3::zero;
    return tgt::make_unique<Volume>(new VolumeDiskLZ4(std::move(*this).moveToHeap()), spacing, offset);
}

const LZ4SliceVolumeMetadataFull& LZ4SliceVolumeBase::getMetaData() const {
    return metadata_;
}

const tgt::svec3& LZ4SliceVolumeBase::getDimensions() const {
    return metadata_.dimensions_;
}

size_t LZ4SliceVolumeBase::getNumSlices() const {
    return getDimensions().z;
}

const std::string& LZ4SliceVolumeBase::getFilePath() const {
    return filePath_;
}

/// Helper function ------------------------------------------------------------

LZ4SliceVolume<uint8_t> binarizeVolume(const VolumeBase& volume, float binarizationThresholdSegmentationNormalized) {
    const auto dimensions = volume.getDimensions();

    LZ4SliceVolumeBuilder<uint8_t> builder(VoreenApplication::app()->getUniqueTmpFilePath("." + LZ4SliceVolumeBase::FILE_EXTENSION), LZ4SliceVolumeMetadata(dimensions));

    for(size_t z = 0; z<dimensions.z; ++z) {
        //progress.setProgress(static_cast<float>(z)/dimensions.z);

        std::unique_ptr<const VolumeRAM> inSlice(volume.getSlice(z));
        auto outSlice(builder.getNextWritableSlice());

        for(size_t y = 0; y<dimensions.y; ++y) {
            for(size_t x = 0; x<dimensions.x; ++x) {
                if(inSlice->getVoxelNormalized(x,y,0) > binarizationThresholdSegmentationNormalized) {
                    outSlice->voxel(x,y,0) = 1;
                } else {
                    outSlice->voxel(x,y,0) = 0;
                }
            }
        }
    }

    return (std::move(builder)).finalize();
}

}
