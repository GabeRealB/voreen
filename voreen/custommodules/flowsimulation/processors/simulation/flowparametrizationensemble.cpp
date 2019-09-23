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

#include "flowparametrizationensemble.h"

namespace voreen {

const std::string FlowParametrizationEnsemble::loggerCat_("voreen.flowsimulation.FlowParametrizationEnsemble");

FlowParametrizationEnsemble::FlowParametrizationEnsemble()
    : Processor()
    , outport_(Port::OUTPORT, "outport", "Parameter Inport")
    , ensembleName_("ensembleName", "Ensemble Name", "test_ensemble")
    , simulationTime_("simulationTime", "Simulation Time (s)", 2.0f, 0.1f, 20.0f)
    , numTimeSteps_("numTimeSteps", "Num. Output Time Steps", 50, 1, 1000)
    , outputResolution_("outputResolution", "Max. Output Resolution", 128, 32, 1024)
    , flowFeatures_("flowFeatures", "Flow Features")
{
    addPort(outport_);

    addProperty(ensembleName_);
    ensembleName_.setGroupID("ensemble");
    addProperty(simulationTime_);
    simulationTime_.setGroupID("ensemble");
    addProperty(numTimeSteps_);
    numTimeSteps_.setGroupID("ensemble");
    addProperty(outputResolution_);
    outputResolution_.setGroupID("ensemble");

    addProperty(flowFeatures_);
    addFeature("Velocity", FF_VELOCITY);
    addFeature("Magnitude", FF_MAGNITUDE);
    addFeature("Pressure", FF_PRESSURE);
    addFeature("Wall Shear Stress", FF_WALLSHEARSTRESS);
    flowFeatures_.addInstance("Velocity"); // Default selection.
    flowFeatures_.setGroupID("ensemble");
    setPropertyGroupGuiName("ensemble", "Ensemble");
}

void FlowParametrizationEnsemble::process() {

    FlowParametrizationList* flowParametrizationList = new FlowParametrizationList(ensembleName_.get());
    flowParametrizationList->setSimulationTime(simulationTime_.get());
    flowParametrizationList->setNumTimeSteps(numTimeSteps_.get());
    flowParametrizationList->setOutputResolution(outputResolution_.get());

    int flowFeatures = FF_NONE;
    for(const InteractiveListProperty::Instance& instance : flowFeatures_.getInstances()) {
        flowFeatures |=  flowFeatureIds_[instance.getItemId()];
    }
    flowParametrizationList->setFlowFeatures(flowFeatures);

    outport_.setData(flowParametrizationList);

}

void FlowParametrizationEnsemble::addFeature(const std::string &name, int id) {
    flowFeatures_.addItem(name);
    flowFeatureIds_.push_back(id);
}

}   // namespace