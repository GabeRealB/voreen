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

#include "fieldparallelplotcreator.h"

#include "tgt/immediatemode/immediatemode.h"
#include "tgt/textureunit.h"

#include "voreen/core/voreenapplication.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"
#include "voreen/core/io/progressbar.h"
#include "voreen/core/ports/conditions/portconditionvolumetype.h"
#include "voreen/core/utils/glsl.h"

#include "../utils/ensemblehash.h"

#include <random>

namespace voreen {

const std::string FieldParallelPlotCreator::loggerCat_("voreen.ensembleanalysis.FieldParallelPlotCreator");

const std::string FieldParallelPlotCreator::META_DATA_HASH("EnsembleHash");

FieldParallelPlotCreator::FieldParallelPlotCreator()
    : AsyncComputeProcessor<ComputeInput, ComputeOutput>()
    , inport_(Port::INPORT, "volumehandle.volumehandle", "Volume Input")
    , seedMask_(Port::INPORT, "seedmask", "Seed Mask Input (optional)")
    , outport_(Port::OUTPORT, "fpp.representation", "FieldPlotData Port")
    , numSeedPoints_("numSeedPoints", "Number of Seed Points", 0, 0, 0)
    , seedTime_("seedTime", "Current Random Seed", static_cast<int>(time(0)), std::numeric_limits<int>::min(), std::numeric_limits<int>::max())
    , verticalResolution_("verticalResolution", "Vertical Resolution", 128, 10, 16384)
    , horizontalResolutionPerTimeUnit_("horizontalResolutionPerTimeUnit", "Horizontal Resolution (Per Time Unit)", 10, 1, 100)
{    
    addPort(inport_);
    ON_CHANGE(inport_, FieldParallelPlotCreator, adjustToEnsemble);
    addPort(seedMask_);
    seedMask_.addCondition(new PortConditionVolumeTypeUInt8());
    addPort(outport_);

    addProperty(numSeedPoints_);
    addProperty(seedTime_);
    addProperty(verticalResolution_);
    addProperty(horizontalResolutionPerTimeUnit_);
}

FieldParallelPlotCreator::~FieldParallelPlotCreator() {
}

Processor* FieldParallelPlotCreator::create() const {
    return new FieldParallelPlotCreator();
}

FieldParallelPlotCreatorInput FieldParallelPlotCreator::prepareComputeInput() {
    const EnsembleDataset* inputPtr = inport_.getThreadSafeData();
    if (!inputPtr)
        throw InvalidInputException("No input", InvalidInputException::S_WARNING);

    if (inputPtr->getMaxNumTimeSteps() < 2)
        throw InvalidInputException("Num Time Steps is 1 or less, no need to aggregate over time.", InvalidInputException::S_WARNING);

    const EnsembleDataset& input = *inputPtr;

    const tgt::Bounds& roi = input.getRoi(); // ROI is defined in physical coordinates.
    if(!roi.isDefined()) {
        throw InvalidInputException("ROI is not defined", InvalidInputException::S_ERROR);
    }

    const VolumeBase* seedMask = seedMask_.getThreadSafeData();
    tgt::Bounds seedMaskBounds;
    tgt::mat4 seedMaskPhysicalToVoxelMatrix;
    if(seedMask) {
        seedMaskBounds = seedMask->getBoundingBox(false).getBoundingBox();
        seedMaskPhysicalToVoxelMatrix = seedMask->getPhysicalToVoxelMatrix();
        LINFO("Restricting seed points to volume mask");
    }

    int tHeight = verticalResolution_.get();
    int tWidth = static_cast<int>(input.getMaxTotalDuration() * horizontalResolutionPerTimeUnit_.get()) + 1;
    int depth = static_cast<int>(input.getCommonChannels().size() * input.getRuns().size());

    std::unique_ptr<FieldPlotData> plotData(new FieldPlotData(tWidth, tHeight, depth));

    std::function<float()> rnd(std::bind(std::uniform_real_distribution<float>(0.0f, 1.0f), std::mt19937(seedTime_.get())));

    std::vector<tgt::vec3> seedPoints(numSeedPoints_.get());
    for (int k = 0; k<numSeedPoints_.get(); k++) {
        seedPoints[k] = tgt::vec3(rnd(), rnd(), rnd());
        seedPoints[k] = tgt::vec3(roi.getLLF()) + seedPoints[k] * tgt::vec3(roi.diagonal());

        // TODO: very rough and dirty restriction, implement something more intelligent.
        if (seedMask && (!seedMaskBounds.containsPoint(seedPoints[k]) ||
          seedMask->getRepresentation<VolumeRAM>()->getVoxelNormalized(seedMaskPhysicalToVoxelMatrix*seedPoints[k]) == 0.0f)) {
            k--; // reseed this point.
            continue;
        }
    }

    return FieldParallelPlotCreatorInput{
            input,
            std::move(plotData),
            seedPoints
    };
}
FieldParallelPlotCreatorOutput FieldParallelPlotCreator::compute(FieldParallelPlotCreatorInput input, ProgressReporter& progress) const {

    progress.setProgress(0.0f);

    const EnsembleDataset& data = input.dataset;
    std::unique_ptr<FieldPlotData> plotData = std::move(input.outputPlot);
    std::vector<tgt::vec3> seedPoints = input.seedPoints;

    const float progressIncrement = 1.0f / (data.getTotalNumTimeSteps() * data.getCommonChannels().size());
    const int pixelPerTimeUnit = horizontalResolutionPerTimeUnit_.get();

    int sliceNumber = 0;
    for (const std::string& channel : data.getCommonChannels()) {
        for (const EnsembleDataset::Run& run : data.getRuns()) {

            const tgt::vec2& valueRange = data.getValueRange(channel);
            progress.setProgressMessage("Plotting channel " + channel + " [" + std::to_string(valueRange.x) + ", " + std::to_string(valueRange.y) + "]");
            float pixelOffset = pixelPerTimeUnit * run.timeSteps_[0].time_;

            for (size_t t = 1; t < run.timeSteps_.size(); t++) {

                float pixel = pixelPerTimeUnit * run.timeSteps_[t - 1].duration_;

                // Extract Volumes to retrieve spacing.
                const VolumeBase* volumePrev = run.timeSteps_[t - 1].channels_.at(channel);
                const VolumeBase* volumeCurr = run.timeSteps_[t].channels_.at(channel);

                // Request RAM representation for data access.
                const VolumeRAM* dataPrev = volumePrev->getRepresentation<VolumeRAM>();
                const VolumeRAM* dataCurr = volumeCurr->getRepresentation<VolumeRAM>();

                int x1 = static_cast<int>(pixelOffset);
                int x2 = static_cast<int>(pixelOffset + pixel);

                for (size_t k = 0; k<seedPoints.size(); k++) {
                    
                    float voxelPrev = dataPrev->getVoxelNormalizedLinear(volumePrev->getPhysicalToVoxelMatrix() * seedPoints[k]);
                    float voxelCurr = dataCurr->getVoxelNormalizedLinear(volumePrev->getPhysicalToVoxelMatrix() * seedPoints[k]);

                    plotData->drawConnection(x1, x2, voxelPrev, voxelCurr, valueRange.x, valueRange.y, sliceNumber);

                    // Add last column separately.
                    if (t == run.timeSteps_.size() - 1) {
                        plotData->putSingleMass(x2, voxelCurr, valueRange.x, valueRange.y, sliceNumber);
                    }
                }

                pixelOffset = pixelOffset + pixel;

                // Update progress.
                progress.setProgress(std::min(progress.getProgress() + progressIncrement, 1.0f));
            }
            sliceNumber++;
        }
    }

    // Add ensemble hash.
    plotData->getVolume()->getMetaDataContainer().addMetaData(FieldParallelPlotCreator::META_DATA_HASH, new StringMetaData(EnsembleHash(data).getHash()));
    
    // We're done here.
    progress.setProgress(1.0f);
    return FieldParallelPlotCreatorOutput{std::move(plotData)};
}

void FieldParallelPlotCreator::processComputeOutput(FieldParallelPlotCreatorOutput output) {
    outport_.setData(output.plotData.release(), true);
}

void FieldParallelPlotCreator::adjustToEnsemble() {

    // Skip if no data available.
    const EnsembleDataset* ensemble = inport_.getData();
    if(!ensemble)
        return;

    numSeedPoints_.setMinValue(1);
    numSeedPoints_.setMaxValue(131072);
    numSeedPoints_.set(32768);
}

bool FieldParallelPlotCreator::isReady() const {
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

} // namespace voreen
