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

#include "streamlinecreator.h"

#include "voreen/core/ports/conditions/portconditionvolumetype.h"
#include "voreen/core/datastructures/volume/volumeminmaxmagnitude.h"

#include "../../datastructures/streamlinelist.h"
#include "../../utils/flowutils.h"

#include <random>

namespace voreen {

const std::string StreamlineCreator::loggerCat_("flowanalysis.StreamlineCreator");

StreamlineCreator::StreamlineCreator()
    : AsyncComputeProcessor()
    , volumeInport_(Port::INPORT, "volInport", "Flow Volume Input (vec3)")
    , seedMask_(Port::INPORT, "seedMaskPort", "Seed Mask (optional)")
    , streamlineOutport_(Port::OUTPORT, "streamlineOutport", "Streamlines Output")
    , numSeedPoints_("numSeedPoints", "Number of Seed Points", 5000, 1, 200000)
    , seedTime_("seedTime", "Current Random Seed", static_cast<int>(time(0)), std::numeric_limits<int>::min(), std::numeric_limits<int>::max())
    , streamlineLengthThreshold_("streamlineLengthThreshold", "Restrict streamline length", tgt::ivec2(10, 1000), 2, 10000)
    , absoluteMagnitudeThreshold_("absoluteMagnitudeThreshold", "Threshold of Magnitude (absolute)", tgt::vec2(0.0f, 1000.0f), 0.0f, 9999.99f)
    , fitAbsoluteMagnitudeThreshold_("fitAbsoluteMagnitude", "Fit absolute Threshold to Input", false)
    , stopIntegrationAngleThreshold_("stopIntegrationAngleThreshold", "Stop Integration on Angle", 180, 0, 180, Processor::INVALID_RESULT, IntProperty::STATIC, Property::LOD_ADVANCED)
    , filterMode_("filterModeProp", "Filtering:", Processor::INVALID_RESULT, false, Property::LOD_DEVELOPMENT)
{
    volumeInport_.addCondition(new PortConditionVolumeChannelCount(3));
    addPort(volumeInport_);
    addPort(seedMask_);
    addPort(streamlineOutport_);

    addProperty(numSeedPoints_);
        numSeedPoints_.setTracking(false);
        numSeedPoints_.setGroupID("streamline");
    addProperty(seedTime_);
        seedTime_.setTracking(false);
        seedTime_.setGroupID("streamline");
    addProperty(streamlineLengthThreshold_);
        streamlineLengthThreshold_.setTracking(false);
        streamlineLengthThreshold_.setGroupID("streamline");
    addProperty(absoluteMagnitudeThreshold_);
        absoluteMagnitudeThreshold_.setTracking(false);
        absoluteMagnitudeThreshold_.setNumDecimals(2);
        absoluteMagnitudeThreshold_.setGroupID("streamline");
    addProperty(fitAbsoluteMagnitudeThreshold_);
        ON_CHANGE(fitAbsoluteMagnitudeThreshold_, StreamlineCreator, adjustPropertiesToInput);
        fitAbsoluteMagnitudeThreshold_.setGroupID("streamline");
    addProperty(stopIntegrationAngleThreshold_);
        stopIntegrationAngleThreshold_.setTracking(false);
        stopIntegrationAngleThreshold_.setGroupID("streamline");
    addProperty(filterMode_);
        filterMode_.addOption("linear", "Linear", VolumeRAM::LINEAR);
        filterMode_.addOption("nearest", "Nearest", VolumeRAM::NEAREST);
        filterMode_.setGroupID("streamline");
    setPropertyGroupGuiName("streamline", "Streamline Settings");
}


bool StreamlineCreator::isReady() const {
    if (!isInitialized()) {
        setNotReadyErrorMessage("Not initialized.");
        return false;
    }
    if (!volumeInport_.isReady()) {
        setNotReadyErrorMessage("Inport not ready.");
        return false;
    }

    // Note: Seed Mask is optional!

    return true;
}

std::vector<std::reference_wrapper<Port>> StreamlineCreator::getCriticalPorts() {
    auto criticalPorts = AsyncComputeProcessor<ComputeInput, ComputeOutput>::getCriticalPorts();
    criticalPorts.erase(std::remove_if(criticalPorts.begin(), criticalPorts.end(), [this] (const std::reference_wrapper<Port>& port){
        return port.get().getID() == seedMask_.getID();
    }), criticalPorts.end());
    return criticalPorts;
}

void StreamlineCreator::adjustPropertiesToInput() {

    const VolumeBase* volume = volumeInport_.getData();
    if(!volume) {
        return;
    }

    if(fitAbsoluteMagnitudeThreshold_.get()) {
        VolumeMinMaxMagnitude* data = volume->getDerivedData<VolumeMinMaxMagnitude>();

        absoluteMagnitudeThreshold_.setMinValue(data->getMinMagnitude());
        absoluteMagnitudeThreshold_.setMaxValue(data->getMaxMagnitude());
        absoluteMagnitudeThreshold_.set(tgt::vec2(data->getMinMagnitude(), data->getMaxMagnitude()));
    }
    else {
        absoluteMagnitudeThreshold_.setMinValue(0.0f);
        absoluteMagnitudeThreshold_.setMaxValue(5000.0f);
    }
}

StreamlineCreatorInput StreamlineCreator::prepareComputeInput() {

    auto flowVolume = volumeInport_.getThreadSafeData();
    if(!flowVolume) {
        throw InvalidInputException("No volume", InvalidInputException::S_ERROR);
    }

    // Set up random generator.
    std::function<float()> rnd(std::bind(std::uniform_real_distribution<float>(0.0f, 1.0f), std::mt19937(seedTime_.get())));

    const tgt::mat4 physicalToVoxelMatrix = flowVolume->getPhysicalToVoxelMatrix();
    const tgt::Bounds roi = flowVolume->getBoundingBox(false).getBoundingBox(false);
    auto numSeedPoints = static_cast<size_t>(numSeedPoints_.get());

    auto seedMask = seedMask_.getData();
    std::vector<tgt::vec3> seedPoints;
    seedPoints.reserve(numSeedPoints_.get());
    if (seedMask) {
        tgt::Bounds roiBounds = roi;
        tgt::Bounds seedMaskBounds = seedMask->getBoundingBox(false).getBoundingBox(false);

        roiBounds.intersectVolume(seedMaskBounds);
        if(!roiBounds.isDefined()) {
            throw InvalidInputException("Seed Mask does not overlap with ensemble ROI", InvalidInputException::S_ERROR);
        }

        VolumeRAMRepresentationLock seedMaskLock(seedMask);

        VolumeMinMax* vmm = seedMask->getDerivedData<VolumeMinMax>();
        if(vmm->getMinNormalized() == 0.0f && vmm->getMaxNormalized() == 0.0f) {
            throw InvalidInputException("Seed Mask is empty", InvalidInputException::S_ERROR);
        }

        tgt::svec3 dim = seedMaskLock->getDimensions();
        std::vector<tgt::vec3> maskVoxels;
        for(size_t z=0; z < dim.z; z++) {
            for(size_t y=0; y < dim.y; y++) {
                for(size_t x=0; x < dim.x; x++) {
                    if(seedMaskLock->getVoxelNormalized(x, y, z) != 0.0f) {
                        maskVoxels.emplace_back(tgt::vec3(x, y, z));
                    }
                }
            }
        }

        if (maskVoxels.empty()) {
            throw InvalidInputException("No seed points found in ROI", InvalidInputException::S_ERROR);
        }

        // If we have more seed mask voxel than we want to have seed points, reduce the list size.
        float probability = static_cast<float>(numSeedPoints) / maskVoxels.size();
        tgt::mat4 seedMaskVoxelToPhysicalMatrix = seedMask->getVoxelToPhysicalMatrix();
        for(const tgt::vec3& seedPoint : maskVoxels) {
            // Determine for each seed point, if we will keep it.
            if(probability >= 1.0f || rnd() < probability) {
                seedPoints.push_back(physicalToVoxelMatrix * (seedMaskVoxelToPhysicalMatrix * seedPoint));
            }
        }

        LINFO("Restricting seed points to volume mask using " << seedPoints.size() << " seeds");
    }
    else {
        // Without a seed mask, we uniformly sample the whole space enclosed by the roi.
        for (size_t k = 0; k<numSeedPoints; k++) {
            tgt::vec3 seedPoint;
            seedPoint = tgt::vec3(rnd(), rnd(), rnd());
            seedPoint = roi.getLLF() + seedPoint * roi.diagonal();
            seedPoints.push_back(physicalToVoxelMatrix * seedPoint);
        }
    }

    tgtAssert(!seedPoints.empty(), "no seed points found");
    if (seedPoints.empty()) {
        throw InvalidInputException("No seed points found", InvalidInputException::S_ERROR);
    }

    std::unique_ptr<StreamlineListBase> output(new StreamlineList(flowVolume));

    return StreamlineCreatorInput {
            streamlineLengthThreshold_.get(),
            absoluteMagnitudeThreshold_.get(),
            stopIntegrationAngleThreshold_.get() * tgt::PIf / 180.0f,
            filterMode_.getValue(),
            volumeInport_.getThreadSafeData(),
            seedMask_.getThreadSafeData(),
            std::move(seedPoints),
            std::move(output)
    };
}

StreamlineCreatorOutput StreamlineCreator::compute(StreamlineCreatorInput input, ProgressReporter& progressReporter) const {

    PortDataPointer<VolumeBase> flowVolume = std::move(input.flowVolume);
    VolumeRAMRepresentationLock representation(flowVolume);
    //const VolumeBase* seedMask_ = input.seedMask; // Currently not used.
    std::vector<tgt::vec3> seedPoints = std::move(input.seedPoints);
    std::unique_ptr<StreamlineListBase> output = std::move(input.output);

    // We use half the steps we had before.
    tgt::vec3 stepSize = 0.5f * flowVolume->getSpacing() / tgt::max(flowVolume->getSpacing());

    size_t lowerLengthThreshold = input.streamlineLengthThreshold.x;
    size_t upperLengthThreshold = input.streamlineLengthThreshold.y;

    const IntegrationInput integrationInput{
        tgt::vec3(representation->getDimensions() - tgt::svec3::one),
        stepSize,
        flowVolume->getVoxelToWorldMatrix(),
        upperLengthThreshold,
        input.absoluteMagnitudeThreshold,
        input.stopIntegrationAngleThreshold
    };

    const SpatialSampler sampler(*representation, flowVolume->getRealWorldMapping(), input.filterMode);

    ThreadedTaskProgressReporter progress(progressReporter, seedPoints.size());
    bool aborted = false;

#ifdef VRN_MODULE_OPENMP
    #pragma omp parallel for
    for (long i=0; i<static_cast<long>(seedPoints.size()); i++) {
        if (aborted) {
            continue;
        }
#else
    for(size_t i=0; i<seedPoints.size(); i++) {
#endif

        const tgt::vec3& start = seedPoints[i];

        Streamline streamline = integrateStreamline(start, sampler, integrationInput);
        if (streamline.getNumElements() >= lowerLengthThreshold) {
#ifdef VRN_MODULE_OPENMP
            #pragma omp critical
#endif
            output->addStreamline(streamline);
        }

        if (progress.reportStepDone()) {
#ifdef VRN_MODULE_OPENMP
            #pragma omp critical
            aborted = true;
#else
            aborted = true;
            break;
#endif
        }
    }

    if (aborted) {
        throw boost::thread_interrupted();
    }

    return StreamlineCreatorOutput {
        std::move(output)
    };
}

void StreamlineCreator::processComputeOutput(StreamlineCreatorOutput output) {
    streamlineOutport_.setData(output.streamlines.release());
}

/*
void StreamlineCreator::reseedPosition(size_t currentPosition) {

    tgt::vec3 randVec = tgt::vec3(rnd(), rnd(), rnd());
    tgt::vec3 dimAsVec3 = tgt::vec3(flow_->getDimensions() - tgt::svec3::one);

    // Flip a coin to determine whether a completely new random position
    // will be taken or wether an exisiting one will be used.
    // When the probability for a new position is low, the seeding positions
    // seem be prone to cluster at single location.
    if ((rnd() < 0.5) || (currentPosition <= 1)) {
        seedingPositions_[currentPosition] = randVec * dimAsVec3;
        return;
    }

    // if there are already random positions which lead to a vector-field value not being
    // zero or which falls within the limits defined by thresholds, take this
    // position to generate another.
    // Use the position and add some random offset to it.
    size_t index = tgt::iround(rnd() * (currentPosition - 1));
    randVec *= rnd() * 9.f + 1.f;
    seedingPositions_[currentPosition] = tgt::clamp((seedingPositions_[index] + randVec), tgt::vec3::zero, dimAsVec3);
}
*/

Streamline StreamlineCreator::integrateStreamline(const tgt::vec3& start, const SpatialSampler& sampler, const IntegrationInput& input) const {

    const float epsilon = 1e-5f; // std::numeric_limits<float>::epsilon() is not enough.

    // Position.
    tgt::vec3 r(start);
    tgt::vec3 r_(start);

    // Velocity.
    tgt::vec3 velR = sampler.sample(r);
    tgt::vec3 velR_ = velR;

    // Return an empty line in case the initial velocity was zero already.
    if(velR == tgt::vec3::zero) {
        return Streamline();
    }

    // Resulting streamline.
    Streamline line;
    line.addElementAtEnd(Streamline::StreamlineElement(input.voxelToWorldMatrix * r, velR));

    bool lookupPositiveDirection = true;
    bool lookupNegativeDirection = true;

    // Look up positive and negative direction in alternating fashion.
    while (lookupPositiveDirection || lookupNegativeDirection) {

        if (lookupPositiveDirection) {

            // Execute 4th order Runge-Kutta step.
            tgt::vec3 k1 = tgt::normalize(velR) * input.stepSize; //v != zero
            tgt::vec3 k2 = sampler.sample(r + (k1 / 2.0f));
            if (k2 != tgt::vec3::zero) k2 = tgt::normalize(k2) * input.stepSize;
            tgt::vec3 k3 = sampler.sample(r + (k2 / 2.0f));
            if (k3 != tgt::vec3::zero) k3 = tgt::normalize(k3) * input.stepSize;
            tgt::vec3 k4 = sampler.sample(r + k3);
            if (k4 != tgt::vec3::zero) k4 = tgt::normalize(k4) * input.stepSize;
            r += ((k1 / 6.0f) + (k2 / 3.0f) + (k3 / 3.0f) + (k4 / 6.0f));

            // Check constrains.
            lookupPositiveDirection &= (r == tgt::clamp(r, tgt::vec3::zero, input.dimensions)); // Ran out of bounds?
            lookupPositiveDirection &= (r != line.getLastElement().position_); // Progress in current direction?

            velR = sampler.sample(r);
            float magnitude = tgt::length(velR);
            lookupPositiveDirection &= (velR != tgt::vec3::zero)
                    && (magnitude > input.absoluteMagnitudeThreshold.x - epsilon)
                    && (magnitude < input.absoluteMagnitudeThreshold.y + epsilon)
                    && std::acos(std::abs(tgt::dot(line.getLastElement().velocity_, velR)) /
                        (tgt::length(line.getLastElement().velocity_) * magnitude)) <= input.stopIntegrationAngleThreshold;

            if (lookupPositiveDirection) {
                line.addElementAtEnd(Streamline::StreamlineElement(input.voxelToWorldMatrix * r, velR));
                if (line.getNumElements() >= input.upperLengthThreshold) {
                    break;
                }
            }
        }

        if (lookupNegativeDirection) {

            // Execute 4th order Runge-Kutta step.
            tgt::vec3 k1 = tgt::normalize(velR_) * input.stepSize; // velR_ != zero
            tgt::vec3 k2 = sampler.sample(r_ - (k1 / 2.0f));
            if (k2 != tgt::vec3::zero) k2 = tgt::normalize(k2) * input.stepSize;
            tgt::vec3 k3 = sampler.sample(r_ - (k2 / 2.0f));
            if (k3 != tgt::vec3::zero) k3 = tgt::normalize(k3) * input.stepSize;
            tgt::vec3 k4 = sampler.sample(r_ - k3);
            if (k4 != tgt::vec3::zero) k4 = tgt::normalize(k4) * input.stepSize;
            r_ -= ((k1 / 6.0f) + (k2 / 3.0f) + (k3 / 3.0f) + (k4 / 6.0f));

            // Check constrains.
            lookupNegativeDirection &= (r_ == tgt::clamp(r_, tgt::vec3::zero, input.dimensions)); // Ran out of bounds?
            lookupNegativeDirection &= (r_ != line.getFirstElement().position_); // Progress in current direction?

            velR_ = sampler.sample(r_);
            float magnitude = tgt::length(velR_);
            lookupNegativeDirection &= (velR_ != tgt::vec3::zero)
                    && (magnitude > input.absoluteMagnitudeThreshold.x - epsilon)
                    && (magnitude < input.absoluteMagnitudeThreshold.y + epsilon)
                    && std::acos(std::abs(tgt::dot(line.getFirstElement().velocity_, velR_)) /
                         (tgt::length(line.getFirstElement().velocity_) * magnitude)) <= input.stopIntegrationAngleThreshold;

            if (lookupNegativeDirection) {
                line.addElementAtFront(Streamline::StreamlineElement(input.voxelToWorldMatrix * r_, velR_));
                if (line.getNumElements() >= input.upperLengthThreshold) {
                    break;
                }
            }
        }
    }

    return line;
}

}   // namespace