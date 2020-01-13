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

#include "vesselgraphselector.h"

#include "voreen/core/datastructures/volume/volume.h"
#include "voreen/core/datastructures/volume/volumelist.h"
#include "voreen/core/processors/processorwidgetfactory.h"
#include <limits.h> //for std::numeric_limits

static const int NUM_DEBUG_VOLUMES_PER_ITERATION = 3;

namespace voreen {

const std::string VesselGraphSelector::loggerCat_("voreen.core.VesselGraphSelector");

VesselGraphSelector::VesselGraphSelector()
    : Processor()
    , inport_(Port::INPORT, "vesselgraphlist.inport", "VesselGraphList Input", false)
    , outport_(Port::OUTPORT, "vesselgraph.outport", "VesselGraph Output", false)
    , graphID_("graphID", "Selected VesselGraph", -1, -1, std::numeric_limits<int>::max()-1)
    , debugVolumeID_("debugVolumeID", "Id of Debug Volume associated with the selected graph", 0, 0, NUM_DEBUG_VOLUMES_PER_ITERATION-1)
    , resultingDebugVesselGraphSelectorID_("resultingDebugVesselGraphSelectorID", "Link with VesselGraphSelector to select correct debug volume generated by VesselGraphCreator", -1, -1, std::numeric_limits<int>::max()-1)
{
    addPort(inport_);
    addPort(outport_);

    addProperty(graphID_);
        ON_CHANGE(graphID_, VesselGraphSelector, syncResultingDebugVesselGraphSelectorID);
    addProperty(debugVolumeID_);
        ON_CHANGE(debugVolumeID_, VesselGraphSelector, syncResultingDebugVesselGraphSelectorID);
    addProperty(resultingDebugVesselGraphSelectorID_);
        resultingDebugVesselGraphSelectorID_.setReadOnlyFlag(true);
}

Processor* VesselGraphSelector::create() const {
    return new VesselGraphSelector();
}

void VesselGraphSelector::process() {
    // processor is only ready, if inport contains a volumelist
    // but the list can be empty
    if(graphID_.get() == -1) {
        outport_.setData(nullptr);
    } else {
        outport_.setData(&inport_.getData()->at(graphID_.get()), false);
    }
}

void VesselGraphSelector::adjustPropertiesToInput() {
    auto input = inport_.getData();
    if(input && !input->empty()) {
        graphID_.setMinValue(0);
        graphID_.setMaxValue(input->size()-1);

        // set to first volume if no volume was present earlier
        if (graphID_.get() == -1) {
            graphID_.set(0);
        }
    } else {
        graphID_.setMinValue(-1);
        graphID_.setMaxValue(-1);
        graphID_.set(-1);
    }
    syncResultingDebugVesselGraphSelectorID();
}

void VesselGraphSelector::syncResultingDebugVesselGraphSelectorID() {
    int val = NUM_DEBUG_VOLUMES_PER_ITERATION*graphID_.get() + debugVolumeID_.get();
    if(val < 0) {
        val = -1;
    }
    resultingDebugVesselGraphSelectorID_.set(val);
}

} // namespace
