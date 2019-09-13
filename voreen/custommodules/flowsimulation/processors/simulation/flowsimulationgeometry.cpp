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

#include "flowsimulationgeometry.h"

#include "include/voreen/core/datastructures/geometry/glmeshgeometry.h"

namespace voreen {

const std::string FlowSimulationGeometry::loggerCat_("voreen.flowsimulation.FlowSimulationGeometry");

FlowSimulationGeometry::FlowSimulationGeometry()
    : Processor()
    , flowParametrizationInport_(Port::INPORT, "flowparametrization.inport", "Flow Parametrization Input")
    , flowParametrizationOutport_(Port::OUTPORT, "flowparametrization.outport", "Flow Parametrization Output")
    , geometryPort_(Port::OUTPORT, "geometry", "Geometry Port")
    , geometryType_("geometryType", "Geometry Type")
    , ratio_("ratio", "Ratio", 1.0f, 0.1f, 10.0f)
    , transformation_("transformation", "Transformation", tgt::mat4::identity)
{
    addPort(flowParametrizationInport_);
    addPort(flowParametrizationOutport_);
    addPort(geometryPort_);

    addProperty(geometryType_);
    geometryType_.addOption("cylinder", "Cylinder", FSGT_CYLINDER);
    addProperty(ratio_);
    addProperty(transformation_);
}

void FlowSimulationGeometry::process() {

    auto* flowParametrizationList = new FlowParametrizationList(*flowParametrizationInport_.getData());
    auto* geometry = new GlMeshGeometryUInt32Normal();

    switch(geometryType_.getValue()) {
    case FSGT_CYLINDER: {

        geometry->setCylinderGeometry(tgt::vec4::one, 1.0f, ratio_.get(), 1.0f, 32, 32, false, false);
        geometry->setTransformationMatrix(transformation_.get());

        FlowIndicator inlet;
        inlet.direction_ = FD_IN;
        inlet.startPhaseFunction_ = FF_SINUS;
        inlet.startPhaseDuration_ = 0.25f;
        inlet.center_ = transformation_.get() * tgt::vec3(0.0f, 0.0f, 0.0f);
        inlet.normal_ = transformation_.get() * tgt::vec3(0.0f, 0.0f, 1.0f);
        inlet.radius_ = 1.0f;
        flowParametrizationList->addFlowIndicator(inlet);

        FlowIndicator outlet;
        outlet.direction_ = FD_OUT;
        outlet.center_ = transformation_.get() * tgt::vec3(0.0f, 0.0f, 1.0f);
        outlet.normal_ = transformation_.get() * tgt::vec3(0.0f, 0.0f, 1.0f);
        outlet.radius_ = ratio_.get();
        flowParametrizationList->addFlowIndicator(outlet);

        break;
    }
    default:
        tgtAssert(false, "Unhandled geometry type");
        break;
    }

    flowParametrizationOutport_.setData(flowParametrizationList);
    geometryPort_.setData(geometry);
}

}   // namespace
