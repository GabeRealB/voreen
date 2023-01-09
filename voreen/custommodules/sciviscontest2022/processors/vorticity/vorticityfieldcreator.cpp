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

#include "vorticityfieldcreator.h"

namespace voreen {

const std::string VorticityFieldCreator::loggerCat_ = "VorticityFieldCreator";

VorticityFieldCreator::VorticityFieldCreator() 
    : Processor()
    , jacobianInport_( Port::INPORT, "jacobianInport", "Jacobian of the volume of interest" )
    , outputVolume_( Port::OUTPORT, "outputVolume", "Vorticity vector field." )
{

    addPort(jacobianInport_);
    addPort(outputVolume_);

}

void VorticityFieldCreator::process() {
    auto inputVolume = jacobianInport_.getData();
    VolumeRAMRepresentationLock jacobianVolume(inputVolume);
    const auto* jacobianVolumeData = dynamic_cast<const VolumeRAM_Mat3Float*>(*jacobianVolume);
    if (jacobianVolumeData == nullptr)
        throw std::runtime_error("Expected jacobian or acceleration volume as inport!");
    tgt::Vector3<long> dimensions = inputVolume->getDimensions();
    RealWorldMapping rwm = inputVolume->getRealWorldMapping();

    auto vorticityVolume = new VolumeRAM_3xFloat(inputVolume->getDimensions());

#ifdef VRN_MODULE_OPENMP
#pragma omp parallel for
#endif
    for (long z = 0; z < dimensions.z; z++) {
        for (long y = 0; y < dimensions.y; y++) {
            for (long x = 0; x < dimensions.x; x++) {
                tgt::svec3 pos(x, y, z);

                auto jacobian = jacobianVolumeData->voxel(pos);

                // dvx/dx  dvx/dy  dvx/dz
                // dvy/dx  dvy/dy  dvy/dz
                // dvz/dx  dvz/dy  dvz/dz

                auto dvzdy = jacobian.t21;
                auto dvydz = jacobian.t12;
                auto dvxdz = jacobian.t02;
                auto dvzdx = jacobian.t20;
                auto dvydx = jacobian.t10;
                auto dvxdy = jacobian.t01;

                vorticityVolume->voxel(pos) = tgt::vec3{
                    // dvz/dy - dvy/dz
                    dvzdy - dvydz,
                    // dvx/dz - dvz/dx
                    dvxdz - dvzdx,
                    // dvy/dx - dvx/dy
                    dvydx - dvxdy
                };
            }
        }
    }

    auto* volume = new Volume(vorticityVolume, inputVolume);
    volume->setRealWorldMapping(RealWorldMapping()); // Override to default rwm.
    volume->setModality(Modality("vorticity"));
    outputVolume_.setData(volume);
}

}
