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

#include "voreen/core/properties/volumeinfoproperty.h"

#include "voreen/core/datastructures/volume/volume.h"

namespace voreen {

const std::string VolumeInfoProperty::loggerCat_("voreen.VolumeInfoProperty");

VolumeInfoProperty::VolumeInfoProperty(const std::string& id, const std::string& guiText, int invalidationLevel, Property::LevelOfDetail lod)
    : Property(id, guiText, invalidationLevel, lod)
    , volume_(0)
{}

VolumeInfoProperty::VolumeInfoProperty()
    : Property("", "", Processor::INVALID_RESULT, Property::LOD_DEFAULT)
    , volume_(0)
{}

Property* VolumeInfoProperty::create() const {
    return new VolumeInfoProperty();
}

void VolumeInfoProperty::reset() {
    setVolume(0);
};

void VolumeInfoProperty::setVolume(const VolumeBase* handle) {
    volume_ = handle;
    updateWidgets();
}

const VolumeBase* VolumeInfoProperty::getVolume() const {
    return volume_;
}

} // namespace voreen
