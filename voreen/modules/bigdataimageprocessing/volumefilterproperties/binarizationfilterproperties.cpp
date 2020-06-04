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

#include "binarizationfilterproperties.h"
#include "../volumefiltering/slicereader.h"

namespace voreen {

BinarizationFilterProperties::BinarizationFilterProperties()
    : threshold_(getId("threshold"), "Binarization Threshold", 0.5f, 0.0f, 1.0f, Processor::INVALID_RESULT, FloatProperty::DYNAMIC)
{
    // Store default settings.
    storeInstance(DEFAULT_SETTINGS);

    // Add properties to list.
    addProperties();
}

std::string BinarizationFilterProperties::getVolumeFilterName() const {
    return "Binarization";
}

void BinarizationFilterProperties::adjustPropertiesToInput(const SliceReaderMetaData& input) {
    const auto& mm = input.estimateMinMax();

    threshold_.setMinValue(mm.x);
    threshold_.setMaxValue(mm.y);
}

VolumeFilter* BinarizationFilterProperties::getVolumeFilter(const SliceReaderMetaData& inputmetadata, int instanceId) const {
    if (instanceSettings_.find(instanceId) == instanceSettings_.end()) {
        return nullptr;
    }
    Settings settings = instanceSettings_.at(instanceId);

    return new BinarizationFilter(inputmetadata.getRealworldMapping().realWorldToNormalized(settings.threshold_));
}
void BinarizationFilterProperties::restoreInstance(int instanceId) {
    auto iter = instanceSettings_.find(instanceId);
    if (iter == instanceSettings_.end()) {
        instanceSettings_[instanceId] = instanceSettings_[DEFAULT_SETTINGS];
    }

    Settings settings = instanceSettings_[instanceId];
    threshold_.set(settings.threshold_);
}
void BinarizationFilterProperties::storeInstance(int instanceId) {
    Settings& settings = instanceSettings_[instanceId];
    settings.threshold_ = threshold_.get();
}
void BinarizationFilterProperties::removeInstance(int instanceId) {
    instanceSettings_.erase(instanceId);
}
void BinarizationFilterProperties::addProperties() {
    properties_.push_back(&threshold_);
}
void BinarizationFilterProperties::serialize(Serializer& s) const {
    s.serialize(getId("instanceSettings"), instanceSettings_);
}
void BinarizationFilterProperties::deserialize(Deserializer& s) {
    try {
        s.deserialize(getId("instanceSettings"), instanceSettings_);
    }
    catch (SerializationException&) {
        s.removeLastError();
        LERROR("You need to reconfigure " << getVolumeFilterName() << " instances of " << ( properties_[0]->getOwner() ? properties_[0]->getOwner()->getGuiName() : "VolumeFilterList"));
    }
}
std::vector<int> BinarizationFilterProperties::getStoredInstances() const {
    std::vector<int> output;
    for(auto& kv : instanceSettings_) {
        if(kv.first != DEFAULT_SETTINGS) {
            output.push_back(kv.first);
        }
    }
    return output;
}

void BinarizationFilterProperties::Settings::serialize(Serializer& s) const {
    s.serialize("threshold", threshold_);
}
void BinarizationFilterProperties::Settings::deserialize(Deserializer& s) {
    s.deserialize("threshold", threshold_);
}

}
