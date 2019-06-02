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

#include "implicitrepresentation.h"

#include "voreen/core/datastructures/volume/volumeatomic.h"

#include "../../utils/geometryconverter.h"

#include <olb3D.h>
using namespace olb;
typedef double T;

namespace voreen {

const std::string ImplicitRepresentation::loggerCat_("voreen.flowreen.FlowGeometrySource");

ImplicitRepresentation::ImplicitRepresentation()
    : Processor()
    , inport_(Port::INPORT, "flowgeomertysource.inport", "")
    , outport_(Port::OUTPORT, "flowgeometrysource.outport", "ID Volume Output", false)
    , method_("method", "Method")
    , dimensions_("dimensions", "Dimensions", 256, 32, 1024)
    , path_("path", "STL geometry", "Path", "", "STL (*.stl)", FileDialogProperty::OPEN_FILE)
{
    addPort(inport_);
    addPort(outport_);

    addProperty(method_);
        method_.addOption("fast", "Fast", 0);
        method_.addOption("accurate", "Accurate", 1);
    addProperty(dimensions_);
    addProperty(path_);
}

Processor* ImplicitRepresentation::create() const {
    return new ImplicitRepresentation();
}

void ImplicitRepresentation::process() {

    const Geometry* inputGeometry = inport_.getData();
    tgtAssert(inputGeometry, "No input");
    if (!exportGeometryToSTL(inputGeometry, path_.get())) {
        LERROR("Failed to export mesh.");
        outport_.setData(nullptr);
        return;
    }

    T spacing = tgt::max(inputGeometry->getBoundingBox(true).diagonal()) / dimensions_.get();
    STLreader<T> stlReader(path_.get(), spacing, 1.0, method_.getValue());

    tgt::Vector3<T> offset = tgt::Vector3<T>::fromPointer(stlReader.getMin().data);

    VolumeRAM_UInt8* idVolume = new VolumeRAM_UInt8(tgt::svec3(dimensions_.get()));
    idVolume->clear(); // Set every voxel to outside (0)
    const float INSIDE = 1.0f;

    std::vector<Octree<T>*> leafs;
    stlReader.getTree()->getLeafs(leafs);

    // TODO: loop could be parallelized.
    for (auto it = leafs.begin(); it != leafs.end(); ++it) {
        if((*it)->getInside()) {

            tgt::Vector3<T> center = tgt::Vector3<T>::fromPointer((*it)->getCenter().data);
            T radius = (*it)->getRadius();

            tgt::Vector3<T> min = center - radius - offset;
            tgt::Vector3<T> max = center + radius - offset;

            tgt::svec3 llf = tgt::max(min / spacing, tgt::Vector3<T>::zero);
            tgt::svec3 urb = tgt::min(max / spacing, tgt::Vector3<T>(idVolume->getDimensions()));

            VRN_FOR_EACH_VOXEL(i, llf, urb) {
                idVolume->setVoxelNormalized(INSIDE, i);
            }
        }
    }

    Volume* outputVolume = new Volume(idVolume, tgt::vec3(spacing), offset);
    outport_.setData(outputVolume);
}

}