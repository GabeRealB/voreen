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

#include "volumecomparison.h"

#include "voreen/core/datastructures/volume/volumedisk.h"
#include "voreen/core/datastructures/volume/volumeminmax.h"
#include "voreen/core/utils/stringutils.h"

#include "tgt/stopwatch.h"

namespace voreen {

const std::string VolumeComparison::loggerCat_("voreen.VolumeComparison");

VolumeComparison::VolumeComparison()
    : Processor()
    , firstSegmentationVolume_(Port::INPORT, "firstsegmentation", "First Segmentation Volume", false)
    , secondSegmentationVolume_(Port::INPORT, "secondsegmentation", "Second Segmentation Volume", false)
    , useClipRegion_("useClipRegion", "Use Clip Region", false)
    , clipRegion_("clipRegion", "Clip Region", tgt::IntBounds(tgt::ivec3(0), tgt::ivec3(1)), tgt::ivec3(0), tgt::ivec3(1))
    , binarizationThreshold_("binarizationThreshold", "Binarization Threshold", 0.5, 0.0, 1.0)
    , startComputation_("startComputation", "Start Computation")
    , progressProperty_("progressProperty", "Quantification Progress")
    , csvSaveFile_("csvFileProp", "CSV Export Path", "CSV Export Path", ".", "Comma seperated values (*.csv)", FileDialogProperty::SAVE_FILE)
    , saveToCsv_("savetocsv", "Save to CSV")
    , lastSummary_()
    //, channel_("channel", "Quantification Channel", 0, 0, 0, Processor::INVALID_RESULT, NumericProperty<int>::DYNAMIC, Property::LOD_ADVANCED)
    //, quantificationPlot_(Port::OUTPORT, "QuantificationPlot", "Quantification Score Plot")
{
    addPort(firstSegmentationVolume_);
    addPort(secondSegmentationVolume_);
    //addPort(quantificationPlot_);

    addProperty(useClipRegion_);
    clipRegion_.setVisibleFlag(false);
    addProperty(clipRegion_);
    addProperty(startComputation_);

    addProperty(progressProperty_);
    addProgressBar(&progressProperty_);

    addProperty(csvSaveFile_);
    addProperty(saveToCsv_);
    ON_CHANGE(saveToCsv_, VolumeComparison, exportToCSV);
    saveToCsv_.setReadOnlyFlag(true);

    ON_CHANGE(firstSegmentationVolume_, VolumeComparison, adjustToInputVolumes);
    ON_CHANGE(secondSegmentationVolume_, VolumeComparison, adjustToInputVolumes);

    ON_CHANGE(useClipRegion_, VolumeComparison, useClipRegionChanged);

    ON_CHANGE(startComputation_, VolumeComparison, computeQuantification);
}

Processor* VolumeComparison::create() const {
    return new VolumeComparison();
}

bool VolumeComparison::isReady() const {
    return isInitialized() && (firstSegmentationVolume_.isReady() || secondSegmentationVolume_.isReady());
}

void VolumeComparison::process() {
    // nothing

}

void VolumeComparison::initialize() {
    Processor::initialize();

    adjustToInputVolumes();
}

void quantification(const VolumeRAM* slice1, const VolumeRAM* slice2, VolumeComparison::ScanSummary& summary, tgt::svec3 llf, tgt::svec3 urb, RealWorldMapping rwm, float rwThreadhold) {

    float threshold = rwm.realWorldToNormalized(rwThreadhold);


    for (size_t y = llf.y; y <= urb.y; ++y) {
        for (size_t x = llf.x; x <= urb.x; ++x) {
            float v1 = slice1->getVoxelNormalized(x, y, 0);
            float v2 = slice2->getVoxelNormalized(x, y, 0);

            bool foregroundOne = v1 > threshold;
            bool foregroundTwo = v2 > threshold;

            if (foregroundOne && foregroundTwo)
                summary.numForegroundBoth_++;
            if (!foregroundOne && foregroundTwo)
                summary.numForegroundOnlyTwo_++;
            if (foregroundOne && !foregroundTwo)
                summary.numForegroundOnlyOne_++;
            if (!foregroundOne && !foregroundTwo)
                summary.numBackgroundBoth_++;

            float diff = std::abs(v1 - v2);
            summary.sumOfVoxelDiffsSquared_ += diff*diff;
            summary.sumOfVoxelDiffsAbs_ += diff;
        }
    }
}

void VolumeComparison::computeQuantification() {

    setProgress(0.f);
    //quantificationPlot_.clear();

    // get volumes
    const VolumeBase* volume1Ptr = firstSegmentationVolume_.getData();
    const VolumeBase* volume2Ptr = secondSegmentationVolume_.getData();

    tgtAssert(volume1Ptr, "no volume 1");
    tgtAssert(volume2Ptr, "no volume 2");

    const VolumeBase& volume1 = *volume1Ptr;
    const VolumeBase& volume2 = *volume2Ptr;

    // check number of channels, dimensions, and data type of the volumes
    if (volume1.getNumChannels() != 1) {
        LERROR("First volume has more than one channel!");
        return;
    }
    if (volume2.getNumChannels() != 1) {
        LERROR("Second volume has more than one channel!");
        return;
    }

    tgt::svec3 dimensions = volume1.getDimensions();
    if (dimensions != volume2.getDimensions()) {
        LERROR("Volumes are not of the same size!");
        return;
    }

    RealWorldMapping rwm = volume1.getRealWorldMapping();
    if (rwm != volume2.getRealWorldMapping()) {
        LERROR("Real world mappings differ!");
        return;
    }

    lastSummary_ = ScanSummary();

    tgt::Stopwatch timer;
    timer.start();

    // get the quantification dimensions
    tgt::svec3 llf, urb;
    llf = tgt::svec3::zero;

    urb = dimensions - tgt::svec3::one;

    // if the clip region is used, crop our bounds
    if (useClipRegion_.get()) {
        llf = tgt::max(llf, tgt::svec3(clipRegion_.get().getLLF()));
        urb = tgt::min(urb, tgt::svec3(clipRegion_.get().getURB()));
    }

    // we do not know how large a single slice is, so we only load one slice at a time for each volume
    for (size_t z = llf.z; z <= urb.z; ++z) {

        std::unique_ptr<VolumeRAM> slice1(volume1.getSlice(z));
        std::unique_ptr<VolumeRAM> slice2(volume2.getSlice(z));
        tgtAssert(slice1, "No slice 1");
        tgtAssert(slice2, "No slice 2");

        // quantify slice according to the data type
        quantification(slice1.get(), slice2.get(), lastSummary_, llf, urb, rwm, binarizationThreshold_.get());

        setProgress(std::min(0.99f, static_cast<float>(z - llf.z) / static_cast<float>(urb.z - llf.z)));
    }

    setProgress(1.f);

    timer.stop();
    LINFO("Quantification Time: " << (float) timer.getRuntime() / 1000.f << " seconds");

    saveToCsv_.setReadOnlyFlag(false);

    /*
    PlotData* plotData = new PlotData(0, 4);
    plotData->setColumnLabel(0, "Time Step");
    plotData->setColumnLabel(1, "Raw sum of voxels");
    plotData->setColumnLabel(2, "Weighted absolute sum");
    plotData->setColumnLabel(3, "Weighted normalized sum (quantification score)");

    PlotData* normalizedPlotData = new PlotData(0,2);
    normalizedPlotData->setColumnLabel(0, "Time Step");
    normalizedPlotData->setColumnLabel(1, "Weighted normalized sum (quantification score)");

    for (size_t i = static_cast<size_t>(quantificationFrame_.get().x); i <= static_cast<size_t>(quantificationFrame_.get().y); ++i) {
        std::vector<PlotCellValue> v(4);
        v.at(0) = PlotCellValue(static_cast<plot_t>(i));
        v.at(1) = PlotCellValue(static_cast<plot_t>(sumResults.at(i)));
        v.at(2) = PlotCellValue(static_cast<plot_t>(absoluteResults.at(i)));
        v.at(3) = PlotCellValue(static_cast<plot_t>(normalizedResults.at(i)));

        std::vector<PlotCellValue> v2(2);
        v2.at(0) = PlotCellValue(static_cast<plot_t>(i));
        v2.at(1) = PlotCellValue(static_cast<plot_t>(normalizedResults.at(i)));

        bool inserted1 = plotData->insert(v);
        bool inserted2 = normalizedPlotData->insert(v2);
        if (!inserted1 || !inserted2) {
            LERROR("Could not insert data into plot");
            delete plotData;
            delete normalizedPlotData;
            plotData = 0;
            normalizedPlotData = 0;
            break;
        }
    }

    quantificationPlot_.setData(plotData, true);
    normalizedQuantificationPlot_.setData(normalizedPlotData, true);*/
}

void VolumeComparison::adjustToInputVolumes() {
    if (!isInitialized())
        return;

    // new input volumes -> reset results
    lastSummary_ = ScanSummary();
    saveToCsv_.setReadOnlyFlag(true);

    // adjust clipping area to input
    const VolumeBase* firstVol = firstSegmentationVolume_.getData();
    const VolumeBase* secondVol = secondSegmentationVolume_.getData();
    const VolumeBase* someVol = firstVol ? firstVol : secondVol;
    if (firstVol && secondVol) {
        // check if dimensions are the same and adjust clipping area
        if (firstVol->getDimensions() == secondVol->getDimensions())
            clipRegion_.setMaxValue(tgt::ivec3(firstVol->getDimensions() - tgt::svec3::one));
    }
    else if (firstVol)
        clipRegion_.setMaxValue(tgt::ivec3(firstVol->getDimensions() - tgt::svec3::one));
    else if (secondVol)
        clipRegion_.setMaxValue(tgt::ivec3(secondVol->getDimensions() - tgt::svec3::one));

    if(someVol) {
        auto vmm = someVol->getDerivedData<VolumeMinMax>();
        binarizationThreshold_.setMinValue(vmm->getMin());
        binarizationThreshold_.setMaxValue(vmm->getMax());
    }
}

void VolumeComparison::useClipRegionChanged() {
    clipRegion_.setVisibleFlag(useClipRegion_.get());
}

void VolumeComparison::exportToCSV() {
    std::ofstream file(csvSaveFile_.get());
    size_t numVoxels = lastSummary_.totalNumberOfVoxels();
    float avgDiffAbs = lastSummary_.sumOfVoxelDiffsAbs_ / numVoxels;
    float variance = lastSummary_.sumOfVoxelDiffsSquared_ / (numVoxels * numVoxels);
    file << "Number_of_voxels,voxels_in_volume1,voxels_in_volume2,voxels_in_both,dice_score,sumDiffAbs,avgDiffAbs,sumDiffSquared,variance" << std::endl;
    file << numVoxels
        << "," << lastSummary_.numForegroundOnlyOne_
        << "," << lastSummary_.numForegroundOnlyTwo_
        << "," << lastSummary_.numForegroundBoth_
        << "," << lastSummary_.diceScore()
        << "," << lastSummary_.sumOfVoxelDiffsAbs_
        << "," << avgDiffAbs
        << "," << lastSummary_.sumOfVoxelDiffsSquared_
        << "," << variance
        << std::endl;

    file.close();
}

VolumeComparison::ScanSummary::ScanSummary()
    : numForegroundBoth_(0)
    , numForegroundOnlyOne_(0)
    , numForegroundOnlyTwo_(0)
    , numBackgroundBoth_(0)
    , sumOfVoxelDiffsSquared_(0.0f)
    , sumOfVoxelDiffsAbs_(0.0f)
{
}

size_t VolumeComparison::ScanSummary::totalNumberOfVoxels() const {
    return numForegroundBoth_ + numForegroundOnlyOne_ + numForegroundOnlyTwo_ + numBackgroundBoth_;
}
float VolumeComparison::ScanSummary::diceScore() const {
    return 2.0f * numForegroundBoth_ / (numForegroundOnlyOne_ + numForegroundOnlyTwo_ + 2*numForegroundBoth_);
}

} // namespace
