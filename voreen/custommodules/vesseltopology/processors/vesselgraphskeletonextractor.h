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

#ifndef VRN_VESSELGRAPHSKELETONEXTRACTOR
#define VRN_VESSELGRAPHSKELETONEXTRACTOR

#include "voreen/core/processors/processor.h"


#include "voreen/core/ports/geometryport.h"

#include "../ports/vesselgraphport.h"

namespace voreen {

// A processor that extracts a vessel graph from a voxel skeleton volume and annotates
// edges and nodes with features derived from the supplied segmentation.
class VesselGraphSkeletonExtractor : public Processor {
public:
    VesselGraphSkeletonExtractor();
    virtual ~VesselGraphSkeletonExtractor();

    virtual std::string getClassName() const         { return "VesselGraphSkeletonExtractor";      }
    virtual std::string getCategory() const       { return "Geometry"; }
    virtual VoreenSerializableObject* create() const;
    virtual void setDescriptions() {
        setDescription("Retrieves the nodes (as a PointListGeometry) and skeletal lines (as a PointSegmentListGeometryVec3) from a VesselGraph.");
    }
    virtual CodeState getCodeState() const        { return CODE_STATE_EXPERIMENTAL;   }
    virtual bool isReady() const;

    virtual void process();

private:

    // Ports
    VesselGraphPort graphInport_;
    GeometryPort nodeOutport_;
    GeometryPort edgeOutport_;

    static const std::string loggerCat_;
};


} // namespace voreen

#endif // VRN_VESSELGRAPHSKELETONEXTRACTOR
