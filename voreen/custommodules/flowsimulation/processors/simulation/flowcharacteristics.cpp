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

#include "flowcharacteristics.h"

#include "voreen/core/datastructures/volume/volumeminmaxmagnitude.h"
#include "voreen/core/datastructures/callback/memberfunctioncallback.h"
#include "voreen/core/ports/conditions/portconditionvolumelist.h"

namespace voreen {

FlowCharacteristics::FlowCharacteristics()
    : Processor()
    , inport_(Port::INPORT, "parametrization", "Parametrization Input")
    , simulationTime_("simulationTime", "Simulation time (s)", 1.0f, 0.1f, 100.0f, Processor::VALID)
    , temporalResolution_("temporalResolution", "Temporal Resolution (ms)", 3.1f, 1.0f, 200.0f, Processor::VALID)
    , characteristicLength_("characteristicLength", "Characteristic Length (mm)", 22.46f, 0.1f, 1000.0f, Processor::VALID)
    , minVelocity_("minVelocity", "Min. Velocity (mm/s)", 0.0f, 0.0f, 1000.0f, Processor::VALID)
    , maxVelocity_("maxVelocity", "Max. Velocity (mm/s)", 0.0f, 0.0f, 1000.0f, Processor::VALID)
    , resetButton_("resetButton", "Reset") // Invalidation level -> resets values.
{
    addPort(inport_);
    inport_.addCondition(new PortConditionVolumeListEnsemble());
    inport_.addCondition(new PortConditionVolumeListAdapter(new PortConditionVolumeType3xFloat()));

    addProperty(simulationTime_);
    addProperty(temporalResolution_);
    addProperty(characteristicLength_);
    addProperty(minVelocity_);
    addProperty(maxVelocity_);
    addProperty(resetButton_);
}

void FlowCharacteristics::process() {
    const VolumeList* volumeList = inport_.getData();
    tgtAssert(volumeList, "no data");

    float maxLength = 0.0f;
    float minVelocity = std::numeric_limits<float>::max();
    float maxVelocity = 0.0f;

    for (size_t i = 0; i < volumeList->size(); i++) {
        const VolumeBase* volume = volumeList->at(i);
        maxLength = std::max(maxLength, tgt::max(volume->getSpacing() * tgt::vec3(volume->getDimensions())));

        VolumeMinMaxMagnitude* minMax = volume->getDerivedData<VolumeMinMaxMagnitude>();
        minVelocity = std::min(minVelocity, minMax->getMinMagnitude());
        maxVelocity = std::max(maxVelocity, minMax->getMaxMagnitude());
    }

    //simulationTime_.set(volumeList->size() * temporalResolution_.get());
    characteristicLength_.set(maxLength);
    minVelocity_.setMaxValue(maxVelocity * 1.2f); // Allow for 20% adjustments.
    minVelocity_.set(minVelocity);
    maxVelocity_.setMaxValue(maxVelocity * 1.2f); // Allow for 20% adjustments.
    maxVelocity_.set(maxVelocity);
}

}

