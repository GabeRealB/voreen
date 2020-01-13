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

#ifndef VRN_WALLSHEARSTRESSEXTRACTOR_H
#define VRN_WALLSHEARSTRESSEXTRACTOR_H

#include "voreen/core/processors/asynccomputeprocessor.h"
#include "voreen/core/ports/geometryport.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"

namespace voreen {

struct WallShearStressExtractorInput {
    std::string geometryPath;
    const VolumeBase* measuredData;
    float density;
    float viscosity;
    size_t padding;
    std::unique_ptr<VolumeRAM_Float> output;
};

struct WallShearStressExtractorOutput {
    std::unique_ptr<VolumeBase> volume;
};

class WallShearStressExtractor : public AsyncComputeProcessor<WallShearStressExtractorInput, WallShearStressExtractorOutput> {
public:
    WallShearStressExtractor();

    virtual Processor* create() const;
    virtual std::string getClassName() const      { return "WallShearStressExtractor"; }
    virtual std::string getCategory() const       { return "Feature Extraction";       }
    virtual CodeState getCodeState() const        { return CODE_STATE_EXPERIMENTAL;    }

    virtual ComputeInput prepareComputeInput();
    virtual ComputeOutput compute(ComputeInput input, ProgressReporter& progressReporter) const;
    virtual void processComputeOutput(ComputeOutput output);

private:

    // Ports
    VolumePort inputVolume_;
    GeometryPort inputGeometry_;
    VolumePort outputVolume_;

    FloatProperty viscosity_;
    FloatProperty density_;
    IntProperty padding_;

    static const std::string loggerCat_;
};

} // namespace voreen

#endif // VRN_ISOSURFACEEXTRACTOR_H
