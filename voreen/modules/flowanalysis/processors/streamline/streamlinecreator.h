/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
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

#ifndef VRN_STREAMLINECREATOR_H
#define VRN_STREAMLINECREATOR_H

#include "voreen/core/processors/asynccomputeprocessor.h"

#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/progressproperty.h"
#include "voreen/core/properties/numeric/intervalproperty.h"

#include "voreen/core/ports/volumeport.h"
#include "../../ports/streamlinelistport.h"

namespace voreen {

class SpatialSampler;

struct StreamlineCreatorInput {
    tgt::ivec2 streamlineLengthThreshold;
    tgt::vec2 absoluteMagnitudeThreshold;
    float velocityUnitConversion;
    int integrationSteps;
    float stopIntegrationAngleThreshold;
    VolumeRAM::Filter filterMode;
    bool transformVelocities;
    PortDataPointer<VolumeBase> flowVolume;
    const VolumeBase* seedMask;
    std::vector<tgt::vec3> seedPoints;
    std::unique_ptr<StreamlineListBase> output;
};

struct StreamlineCreatorOutput {
    std::unique_ptr<StreamlineListBase> streamlines;
};

/**
 * This processor is used to create streamlines from a vec3 volume.
 * It can be used with the StreamlineRenderer3D. At the moment only RAM volumes are supported.
 */
class VRN_CORE_API StreamlineCreator : public AsyncComputeProcessor<StreamlineCreatorInput, StreamlineCreatorOutput> {
public:
    StreamlineCreator();

    virtual Processor* create() const { return new StreamlineCreator(); }

    virtual std::string getCategory() const { return "Streamline Processing"; }
    virtual std::string getClassName() const { return "StreamlineCreator"; }
    virtual Processor::CodeState getCodeState() const { return CODE_STATE_STABLE; }

    virtual ComputeInput prepareComputeInput();
    virtual ComputeOutput compute(ComputeInput input, ProgressReporter& progressReporter) const;
    virtual void processComputeOutput(ComputeOutput output);

protected:

    virtual bool isReady() const;
    virtual void adjustPropertiesToInput();

    virtual void setDescriptions() {
        setDescription("This processor is used to create streamlines from a vec3 volume. The resulting streamlines can be visualized or modified " \
                        "by other processors of the <i>FlowAnalysis</i> module.");
        numSeedPoints_.setDescription("Can be used to determine the number of streamlines, which should be created. It can be used as a performance parameter.");
        seedTime_.setDescription("It is used as debug output to see the current generator. See the next description for more details.");
        absoluteMagnitudeThreshold_.setDescription("Flow data points outside the threshold intervall will not be used for streamline construction.");
        transformVelocities_.setDescription("If enabled, the velocities are transformed by the volume transformation.");
    }

private:

    struct IntegrationInput {
        float stepSize;
        float velocityUnitConversion;
        size_t upperLengthThreshold;
        tgt::vec2 absoluteMagnitudeThreshold;
        float stopIntegrationAngleThreshold;
        std::function<bool(const tgt::vec3&)> bounds;
    };

    Streamline integrateStreamline(const tgt::vec3& start, const SpatialSampler& sampler, const IntegrationInput& input) const;

    VolumePort volumeInport_;
    VolumePort seedMask_;
    StreamlineListPort streamlineOutport_;

    // seeding
    IntProperty numSeedPoints_;                             ///< number of seed points
    IntProperty seedTime_;                                  ///< seed

    // streamline settings
    IntIntervalProperty streamlineLengthThreshold_;     ///< restrict number of elements
    FloatIntervalProperty absoluteMagnitudeThreshold_;  ///< only magnitudes in this interval are used
    BoolProperty stopOutsideMask_;                      ///< stop integration if running outside mask
    BoolProperty fitAbsoluteMagnitudeThreshold_;        ///< fit magnitude on input change?
    IntProperty stopIntegrationAngleThreshold_;         ///< stop integration when exceeding threshold?
    OptionProperty<VolumeRAM::Filter> filterMode_;      ///< filtering inside the dataset
    BoolProperty transformVelocities_;                  ///< transform velocities by volume transformation?

    // debug
    FloatOptionProperty velocityUnitConversion_;
    IntProperty integrationSteps_;

    static const std::string loggerCat_;
};

}   // namespace

#endif  // VRN_STREAMLINECREATOR_H
