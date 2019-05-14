/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2017 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
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

#include "similaritymatrixcreator.h"

#include "../utils/ensemblehash.h"
#include "../utils/utils.h"

#include "voreen/core/voreenapplication.h"
#include "voreen/core/datastructures/volume/volume.h"
#include "voreen/core/datastructures/volume/volumeram.h"

#include <random>

namespace voreen {

const std::string SimilarityMatrixCreator::loggerCat_("voreen.ensembleanalysis.SimilarityMatrixCreator");

SimilarityMatrixCreator::SimilarityMatrixCreator()
    : AsyncComputeProcessor<ComputeInput, ComputeOutput>()
    , inport_(Port::INPORT, "inport", "Ensemble Datastructure Input", false)
    , seedMask_(Port::INPORT, "seedmask", "Seed Mask Input (optional)")
    , outport_(Port::OUTPORT, "outport", "Similarity Matrix Output", false)
    , singleChannelSimilarityMeasure_("singleChannelSimilarityMeasure", "Single Channel Similarity Measure")
    , isoValue_("isovalue", "Iso-Value", 0.5f, 0.0f, 1.0f)
    , multiChannelSimilarityMeasure_("multiChannelSimilarityMeasure", "Multi Channel Similarity Measure")
    , weight_("weight", "Weight (0=magnitude, 1=angle)", 0.5f, 0.0f, 1.0f)
    , numSeedPoints_("numSeedPoints", "Number of Seed Points", 0, 0, 0)
    , seedTime_("seedTime", "Current Random Seed", static_cast<int>(time(0)), std::numeric_limits<int>::min(), std::numeric_limits<int>::max())
{
    // Ports
    addPort(inport_);
    addPort(seedMask_);
    addPort(outport_);

    // Calculation
    addProperty(singleChannelSimilarityMeasure_);
    singleChannelSimilarityMeasure_.addOption("isovalue", "Iso-Surface", MEASURE_ISOSURFACE);
    singleChannelSimilarityMeasure_.addOption("multifield", "Multi-Field", MEASURE_MULTIFIELD);
    singleChannelSimilarityMeasure_.set("multifield");
    ON_CHANGE_LAMBDA(singleChannelSimilarityMeasure_, [this] {
        isoValue_.setVisibleFlag(singleChannelSimilarityMeasure_.getValue() == MEASURE_ISOSURFACE);
    });
    addProperty(isoValue_);
    isoValue_.setVisibleFlag(false);

    addProperty(multiChannelSimilarityMeasure_);
    multiChannelSimilarityMeasure_.addOption("magnitude", "Magnitude", MEASURE_MAGNITUDE);
    multiChannelSimilarityMeasure_.addOption("angleDifference", "Angle Difference", MEASURE_ANGLEDIFFERENCE);
    multiChannelSimilarityMeasure_.addOption("magnitudeAndAngleDifference", "Magnitude and Angle Difference", MEASURE_MAGNITUDE_AND_ANGLEDIFFERENCE);
    ON_CHANGE_LAMBDA(multiChannelSimilarityMeasure_, [this] {
        weight_.setVisibleFlag(multiChannelSimilarityMeasure_.getValue() == MEASURE_MAGNITUDE_AND_ANGLEDIFFERENCE);
    });
    addProperty(weight_);
    weight_.setVisibleFlag(false);

    addProperty(numSeedPoints_);
    addProperty(seedTime_);
}

Processor* SimilarityMatrixCreator::create() const {
    return new SimilarityMatrixCreator();
}


bool SimilarityMatrixCreator::isReady() const {
    if (!isInitialized()) {
        setNotReadyErrorMessage("Not initialized.");
        return false;
    }
    if (!inport_.isReady()) {
        setNotReadyErrorMessage("Inport not ready.");
        return false;
    }

    // Note: Seed Mask is optional!

    return true;
}

void SimilarityMatrixCreator::adjustPropertiesToInput() {

    const EnsembleDataset* dataset = inport_.getData();
    if (!dataset)
        return;

    numSeedPoints_.setMinValue(1);
    numSeedPoints_.setMaxValue(131072);
    numSeedPoints_.set(32768);
}

SimilarityMatrixCreatorInput SimilarityMatrixCreator::prepareComputeInput() {
    const EnsembleDataset* inputPtr = inport_.getThreadSafeData();
    if (!inputPtr)
        throw InvalidInputException("No input", InvalidInputException::S_WARNING);

    const EnsembleDataset& input = *inputPtr;

    const tgt::Bounds& roi = input.getRoi(); // ROI is defined in physical coordinates.
    if (!roi.isDefined()) {
        throw InvalidInputException("ROI is not defined", InvalidInputException::S_ERROR);
    }

    for(const std::string& channel : input.getCommonChannels()) {
        size_t numChannels = input.getNumChannels(channel);
        if(numChannels != 1 && numChannels != 3) {
            throw InvalidInputException("Only volumes with 1 or 3 channels supported", InvalidInputException::S_ERROR);
        }
    }

    const VolumeBase* seedMask = seedMask_.getData();
    tgt::Bounds seedMaskBounds;
    tgt::mat4 seedMaskPhysicalToVoxelMatrix;
    if (seedMask) {
        seedMaskBounds = seedMask->getBoundingBox(false).getBoundingBox();
        seedMaskPhysicalToVoxelMatrix = seedMask->getPhysicalToVoxelMatrix();
        LINFO("Restricting seed points to volume mask");
    }

    std::unique_ptr<SimilarityMatrixList> outputMatrices(new SimilarityMatrixList(input));

    std::function<float()> rnd(
            std::bind(std::uniform_real_distribution<float>(0.0f, 1.0f), std::mt19937(seedTime_.get())));

    std::vector<tgt::vec3> seedPoints;
    for (int k = 0; k < numSeedPoints_.get(); k++) {
        tgt::vec3 seedPoint(rnd(), rnd(), rnd());
        seedPoint = tgt::vec3(roi.getLLF()) + seedPoint * tgt::vec3(roi.diagonal());

        // TODO: very rough and dirty restriction, implement something more intelligent.
        if (!seedMask || (seedMaskBounds.containsPoint(seedPoint) &&
                          seedMask->getRepresentation<VolumeRAM>()->getVoxelNormalized(seedMaskPhysicalToVoxelMatrix*seedPoint) != 0.0f)) {
            seedPoints.push_back(seedPoint);
        }
    }

    return SimilarityMatrixCreatorInput{
            input,
            std::move(outputMatrices),
            std::move(seedPoints),
            singleChannelSimilarityMeasure_.getValue(),
            isoValue_.get(),
            multiChannelSimilarityMeasure_.getValue(),
            weight_.get()
    };
}

SimilarityMatrixCreatorOutput SimilarityMatrixCreator::compute(SimilarityMatrixCreatorInput input, ProgressReporter& progress) const {

    std::unique_ptr<SimilarityMatrixList> similarityMatrices = std::move(input.outputMatrices);
    const std::vector<tgt::vec3>& seedPoints = input.seedPoints;

    progress.setProgress(0.0f);

    const std::vector<std::string>& channels = input.dataset.getCommonChannels();
    for (size_t i=0; i<channels.size(); i++) {

        const std::string& channel = channels[i];
        const tgt::vec2& valueRange = input.dataset.getValueRange(channel);
        size_t numChannels = input.dataset.getNumChannels(channel);

        // Init empty flags.
        std::vector<std::vector<std::vector<float>>> Flags(
                input.dataset.getTotalNumTimeSteps(), std::vector<std::vector<float>>(
                        seedPoints.size(), std::vector<float>(
                                numChannels, 0.0f)
                                )
                        );

        SubtaskProgressReporter runProgressReporter(progress, tgt::vec2(i, 0.9f*(i+1))/tgt::vec2(channels.size()));
        float progressPerTimeStep = 1.0f / (input.dataset.getTotalNumTimeSteps());
        size_t index = 0;
        for (const EnsembleDataset::Run& run : input.dataset.getRuns()) {
            for (const EnsembleDataset::TimeStep& timeStep : run.timeSteps_) {

                const VolumeBase* volume = timeStep.channels_.at(channel);
                tgt::mat4 physicalToVoxelMatrix = volume->getPhysicalToVoxelMatrix();
                RealWorldMapping rwm = volume->getRealWorldMapping();

                for (size_t k = 0; k < seedPoints.size(); k++) {
                    for(size_t ch = 0; ch < numChannels; ch++) {
                        float value = volume->getRepresentation<VolumeRAM>()->getVoxelNormalizedLinear(
                                physicalToVoxelMatrix * seedPoints[k], ch);

                        value = rwm.normalizedToRealWorld(value);
                        value = mapRange(value, valueRange.x, valueRange.y, 0.0f, 1.0f);

                        if (input.singleChannelSimilarityMeasure == MEASURE_ISOSURFACE && numChannels == 1) {
                            bool inside = value < input.isoValue;
                            Flags[index][k][ch] = inside ? 1.0f : 0.0f;
                        } else {
                            Flags[index][k][ch] = value;
                        }
                    }
                }

                // Update progress.
                runProgressReporter.setProgress(index * progressPerTimeStep);
                index++;
            }
        }

        ////////////////////////////////////////////////////////////
        //Calculate distances for up-right corner and reflect them//
        ////////////////////////////////////////////////////////////

        SimilarityMatrix& DistanceMatrix = similarityMatrices->getSimilarityMatrix(channel);

#ifdef VRN_MODULE_OPENMP
#pragma omp parallel for shared(Flags), shared(DistanceMatrix)
#endif
        for (long i = 0; i < static_cast<long>(DistanceMatrix.getSize()); i++) {
            for (long j = 0; j <= i; j++) {
                if(numChannels == 1) {

                    float ScaleSum = 0.0f;
                    float resValue = 0.0f;

                    for (size_t k = 0; k < seedPoints.size(); k++) {

                        float a = Flags[i][k][0];
                        float b = Flags[j][k][0];

                        ScaleSum += (1.0f - (a < b ? a : b));
                        resValue += (1.0f - (a > b ? a : b));
                    }

                    if (ScaleSum > 0.0f)
                        DistanceMatrix(i, j) = (ScaleSum - resValue) / ScaleSum;
                    else
                        DistanceMatrix(i, j) = 1.0f;
                }
                else {

                    Statistics magnitudeStatistics(false);
                    Statistics velocityStatistics(false);

                    for (size_t k = 0; k < seedPoints.size(); k++) {

                        tgt::vec4 direction_i = tgt::vec4::zero;
                        tgt::vec4 direction_j = tgt::vec4::zero;

                        for (size_t ch = 0; ch < numChannels; ch++) {
                            direction_i[ch] = Flags[i][k][ch];
                            direction_j[ch] = Flags[j][k][ch];
                        }

                        if(input.multiChannelSimilarityMeasure & MEASURE_MAGNITUDE) {
                            float length_i = tgt::length(direction_i);
                            float length_j = tgt::length(direction_j);

                            float magnitudeDifference = tgt::abs(length_i - length_j);
                            magnitudeStatistics.addSample(magnitudeDifference);
                        }

                        if(input.multiChannelSimilarityMeasure & MEASURE_ANGLEDIFFERENCE) {
                            if (direction_i == tgt::vec4::zero && direction_j == tgt::vec4::zero) {
                                velocityStatistics.addSample(0.0f);
                            }
                            else if (direction_i != tgt::vec4::zero && direction_j != tgt::vec4::zero) {
                                tgt::vec4 normDirection_i = tgt::normalize(direction_i);
                                tgt::vec4 normDirection_j = tgt::normalize(direction_j);

                                float dot = tgt::dot(normDirection_i, normDirection_j);
                                float angle = std::acos(tgt::clamp(dot, -1.0f, 1.0f)) / tgt::PIf;
                                velocityStatistics.addSample(angle);
                            }
                            else {
                                velocityStatistics.addSample(1.0f);
                            }
                        }
                    }

                    if(input.multiChannelSimilarityMeasure == MEASURE_MAGNITUDE) {
                        DistanceMatrix(i, j) = magnitudeStatistics.getMean();
                    }
                    else if(input.multiChannelSimilarityMeasure == MEASURE_ANGLEDIFFERENCE) {
                        DistanceMatrix(i, j) = velocityStatistics.getMean();
                    }
                    else if(input.multiChannelSimilarityMeasure == MEASURE_MAGNITUDE_AND_ANGLEDIFFERENCE) {
                        DistanceMatrix(i, j) =
                                (1.0f - input.weight) * magnitudeStatistics.getMean() +
                                (       input.weight) * velocityStatistics.getMean();
                    }
                    else {
                        tgtAssert(false, "unhandled multi-channel measure");
                        DistanceMatrix(i, j) = 1.0f;
                    }
                }
            }
        }
    }

    progress.setProgress(1.0f);

    return SimilarityMatrixCreatorOutput{
            std::move(similarityMatrices)
    };
}

void SimilarityMatrixCreator::processComputeOutput(ComputeOutput output) {
    outport_.setData(output.outputMatrices.release(), true);
}


} // namespace
