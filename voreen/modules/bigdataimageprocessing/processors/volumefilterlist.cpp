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

#include "volumefilterlist.h"

#include "modules/hdf5/io/hdf5volumereader.h"
#include "modules/hdf5/io/hdf5volumewriter.h"
#include "modules/hdf5/io/hdf5filevolume.h"

// Include actual Volume Filters.
#include "../volumefiltering/binarizationfilter.h"
#include "../volumefiltering/binarymedianfilter.h"
#include "../volumefiltering/gaussianfilter.h"
#include "../volumefiltering/medianfilter.h"
#include "../volumefiltering/morphologyfilter.h"
#include "../volumefiltering/resamplefilter.h"
#include "../volumefiltering/thresholdingfilter.h"

// Include their properties.
#include "../volumefilterproperties/binarizationfilterproperties.h"
#include "../volumefilterproperties/binarymedianfilterproperties.h"
#include "../volumefilterproperties/gaussianfilterproperties.h"
#include "../volumefilterproperties/medianfilterproperties.h"
#include "../volumefilterproperties/morphologyfilterproperties.h"
#include "../volumefilterproperties/resamplefilterproperties.h"
#include "../volumefilterproperties/thresholdingfilterproperties.h"

namespace voreen {

const std::string VolumeFilterList::loggerCat_("voreen.bigdataimageprocessing.VolumeFilterList");

VolumeFilterList::VolumeFilterList()
    : AsyncComputeProcessor<VolumeFilterListInput, VolumeFilterListOutput>()
    , inport_(Port::INPORT, "volumehandle.input", "Volume Input")
    , outport_(Port::OUTPORT, "volumehandle.output", "Volume Output", false)
    , enabled_("enabled", "Enabled", true)
    , outputVolumeFilePath_("outputVolumeFilePath", "Output Volume", "Path", "", "HDF5 (*.h5)", FileDialogProperty::SAVE_FILE, Processor::INVALID_RESULT, Property::LOD_DEFAULT)
    , outputVolumeDeflateLevel_("outputVolumeDeflateLevel", "Deflate Level", 1, 0, 9, Processor::INVALID_RESULT, IntProperty::STATIC, Property::LOD_DEFAULT)
    , filterList_("filterList", "Filter List", true)
    , numInstances_(0)
    , propertyDisabler_(*this)
{
    addPort(inport_);
        ON_CHANGE(inport_, VolumeFilterList, inputOutputChannelCheck);
    addPort(outport_);

    addProperty(filterList_);
        filterList_.setGroupID("filter");
        filterList_.setDuplicationAllowed(true);
        ON_CHANGE(filterList_, VolumeFilterList, onFilterListChange);
    setPropertyGroupGuiName("filter", "Filter");

    // Add filters (this will add their properties!)
    // Note: The items will appear in the order below.
    // Reordering and removal of single items is possible.
    addFilter(new BinarizationFilterProperties());
    addFilter(new BinaryMedianFilterProperties());
    addFilter(new GaussianFilterProperties());
    addFilter(new MedianFilterProperties());
    addFilter(new MorphologyFilterProperties());
    addFilter(new ResampleFilterProperties());
    addFilter(new ThresholdingFilterProperties());

    // Technical stuff.
    addProperty(enabled_);
        enabled_.setGroupID("output");
        ON_CHANGE_LAMBDA(enabled_, [this] () {
            if(enabled_.get()) {
                this->outport_.setData(nullptr);
                propertyDisabler_.restore();
            } else {
                this->forceComputation();
                propertyDisabler_.saveState([this] (Property* p) { return p == &enabled_; });
                propertyDisabler_.disable();
            }
        });
    addProperty(outputVolumeFilePath_);
        outputVolumeFilePath_.setGroupID("output");
    addProperty(outputVolumeDeflateLevel_);
        outputVolumeDeflateLevel_.setGroupID("output");
    setPropertyGroupGuiName("output", "Output");

    propertyDisabler_.saveState([this] (Property* p) { return p == &enabled_; });
}

VolumeFilterList::~VolumeFilterList() {
}

bool VolumeFilterList::isReady() const {
    if(!isInitialized()) {
        setNotReadyErrorMessage("Not initialized.");
        return false;
    }
    if(!inport_.isReady()) {
        setNotReadyErrorMessage("Inport not ready.");
        return false;
    }
    return true;
}

void VolumeFilterList::initialize() {
    AsyncComputeProcessor::initialize();
}

Processor* VolumeFilterList::create() const {
    return new VolumeFilterList();
}

void VolumeFilterList::serialize(Serializer& s) const {
    AsyncComputeProcessor<ComputeInput, ComputeOutput>::serialize(s);
    for(size_t i=0; i < filterProperties_.size(); i++) {
        filterProperties_[i]->serialize(s);
    }
}

void VolumeFilterList::deserialize(Deserializer& s) {
    AsyncComputeProcessor<ComputeInput, ComputeOutput>::deserialize(s);
    for(size_t i=0; i < filterProperties_.size(); i++) {
        // In case a new filter was added, it won't be able to be deserialized.
        try {
            filterProperties_[i]->deserialize(s);
        } catch(SerializationException& e) {
            LWARNING("Failed to deserialize Filterproperty '" << filterProperties_[i]->getVolumeFilterName() << "': " << e.what());
        }
    }

    inputOutputChannelCheck();
}

VolumeFilterListInput VolumeFilterList::prepareComputeInput() {
    if(!enabled_.get()) {
        outport_.setData(inport_.getData(), false);
        throw InvalidInputException("", InvalidInputException::S_IGNORE);
    }

    if(!inport_.hasData()) {
        throw InvalidInputException("No input", InvalidInputException::S_WARNING);
    }

    auto inputVolPtr = inport_.getThreadSafeData();
    const VolumeBase& inputVolume = *inputVolPtr;

    VolumeFilterStackBuilder builder(inputVolume);
    std::string baseType = inputVolume.getBaseType();
    size_t numOutputChannels = inputVolume.getNumChannels();
    for(const InteractiveListProperty::Instance& instance : filterList_.getInstances()) {

        if(!instance.isActive()) {
            LINFO("Filter: '" << instance.getName() << "' is not active. Skipping.");
            continue;
        }

        VolumeFilter* filter = filterProperties_[instance.getItemId()]->getVolumeFilter(inputVolume, instance.getInstanceId());
        if(!filter) {
            LWARNING("Filter: '" << instance.getName() << "' has not been configured yet. Taking default.");
            filter = filterProperties_[instance.getItemId()]->getVolumeFilter(inputVolume, FilterProperties::DEFAULT_SETTINGS);
        }
        tgtAssert(filter, "filter was null");
        tgtAssert(numOutputChannels == filter->getNumInputChannels(), "channel mismatch");

        // Base and number of channels type of output volume is determined by last filter output type.
        baseType = filter->getSliceBaseType();
        numOutputChannels = filter->getNumOutputChannels();

        builder.addLayer(std::unique_ptr<VolumeFilter>(filter));
    }

    std::unique_ptr<SliceReader> sliceReader = builder.build(0);

    // Reset output volume to make sure it (and the hdf5filevolume) are not used any more
    outport_.setData(nullptr);

    const std::string volumeFilePath = outputVolumeFilePath_.get();
    const std::string volumeLocation = HDF5VolumeWriter::VOLUME_DATASET_NAME;
    const tgt::svec3 dim = sliceReader->getDimensions();

    if(volumeFilePath.empty()) {
        throw InvalidInputException("No volume file path specified!", InvalidInputException::S_ERROR);
    }

    std::unique_ptr<HDF5FileVolume> outputVolume = nullptr;
    try {
        outputVolume = std::unique_ptr<HDF5FileVolume>(HDF5FileVolume::createVolume(volumeFilePath, volumeLocation, baseType, dim, numOutputChannels, true, outputVolumeDeflateLevel_.get(), tgt::svec3(dim.x, dim.y, 1), false));
    } catch(tgt::IOException& e) {
        throw InvalidInputException("Could not create output volume.", InvalidInputException::S_ERROR);
    }

    tgt::vec3 scale(tgt::vec3(inputVolume.getDimensions()) / tgt::vec3(dim));
    tgt::vec3 additionalOffset(scale * tgt::vec3(0.5) - tgt::vec3(0.5));

    outputVolume->writeSpacing(inputVolume.getSpacing() * scale);
    outputVolume->writeOffset(inputVolume.getOffset() + additionalOffset * inputVolume.getSpacing());
    outputVolume->writeRealWorldMapping(inputVolume.getRealWorldMapping());
    outputVolume->writePhysicalToWorldTransformation(inputVolume.getPhysicalToWorldMatrix());

    return VolumeFilterListInput(
            std::move(sliceReader),
            std::move(outputVolume)
    );
}
VolumeFilterListOutput VolumeFilterList::compute(VolumeFilterListInput input, ProgressReporter& progressReporter) const {
    tgtAssert(input.sliceReader, "No sliceReader");
    tgtAssert(input.outputVolume, "No outputVolume");

    writeSlicesToHDF5File(*input.sliceReader, *input.outputVolume, &progressReporter);

    return {
        input.outputVolume->getFileName()
    };
    //outputVolume will be destroyed and thus closed now.
}
void VolumeFilterList::processComputeOutput(VolumeFilterListOutput output) {
    // outputVolume has been destroyed and thus closed by now.
    // So we can open it again (and use HDF5VolumeReader's implementation to read all the metadata with the file)
    const VolumeBase* vol = HDF5VolumeReaderOriginal().read(output.outputVolumeFilePath)->at(0);
    outport_.setData(vol);
}

void VolumeFilterList::adjustPropertiesToInput() {
    const VolumeBase* input = inport_.getData();
    if(!input) {
        return;
    }

    for(auto& filterProperties : filterProperties_) {
        filterProperties->adjustPropertiesToInput(*input);
    }
}

// private methods
//

void VolumeFilterList::onFilterListChange() {

    // Check if instance was deleted.
    bool numInstancesChanged = filterList_.getInstances().size() != numInstances_;
    if(numInstancesChanged) {
        // Handle removal.
        if(numInstances_ > filterList_.getInstances().size() && selectedInstance_) {
            // Assumes that only the selected item can be removed!
            tgtAssert(numInstances_ == filterList_.getInstances().size() + 1, "Only single instance removal allowed!");
            setPropertyGroupVisible(filterList_.getItems()[selectedInstance_->getItemId()], false);
            filterProperties_[selectedInstance_->getItemId()]->removeInstance(selectedInstance_->getInstanceId());
            selectedInstance_.reset();
        }
        numInstances_ = filterList_.getInstances().size();
    }

    // Hide old group.
    if(selectedInstance_) {
        filterProperties_[selectedInstance_->getItemId()]->storeVisibility();
        // No need to store the settings here, since it is done on change anyways.
        //filterProperties_[selectedInstance_->itemId_]->storeInstance(selectedInstance_->instanceId_);
        setPropertyGroupVisible(filterList_.getItems()[selectedInstance_->getItemId()], false);

        // We need to reset here, because otherwise onFilterPropertyChange
        // will be triggered while the current instance is restored.
        selectedInstance_.reset();
    }

    // Show new group.
    boost::optional<InteractiveListProperty::Instance> currentInstance;
    if(filterList_.getSelectedInstance() != -1) {
        currentInstance = filterList_.getInstances()[filterList_.getSelectedInstance()];
        setPropertyGroupVisible(filterList_.getItems()[currentInstance->getItemId()], true);
        filterProperties_[currentInstance->getItemId()]->restoreVisibility();
        filterProperties_[currentInstance->getItemId()]->restoreInstance(currentInstance->getInstanceId());
    }

    selectedInstance_ = currentInstance;

    // Check, if channels match at filter interfaces.
    inputOutputChannelCheck();

    // Set min/max values etc. for new filters
    adjustPropertiesToInput();
}

void VolumeFilterList::onFilterPropertyChange() {
    // If any filter property was modified, we need to store the settings immediately.
    if(selectedInstance_) {
        filterProperties_[selectedInstance_->getItemId()]->storeInstance(selectedInstance_->getInstanceId());
    }
}

void VolumeFilterList::inputOutputChannelCheck() {
    if(inport_.hasData()) {
        const VolumeBase& volume = *inport_.getData();
        size_t numOutputChannels = volume.getNumChannels();
        for (InteractiveListProperty::Instance& instance : filterList_.getInstances()) {
            VolumeFilter* filter = filterProperties_[instance.getItemId()]->getVolumeFilter(volume, FilterProperties::DEFAULT_SETTINGS);
            tgtAssert(filter, "filter was null");

            if (numOutputChannels == filter->getNumInputChannels()) {
                instance.setActive(true);
                numOutputChannels = filter->getNumOutputChannels();
            }
            else if(instance.isActive()) {
                instance.setActive(false);
                LERROR("Input channel count of filter '" << instance.getName() << "' is not satisfied. Deactivating.");
            }
        }
    }
    else {
        // Reset filter active state.
        for(InteractiveListProperty::Instance& instance : filterList_.getInstances()) {
            instance.setActive(true);
        }
    }

    // Don't invalidate here, since this will lead to infinite recursion.
    // We just need to update the widgets only, anyways.
    filterList_.updateWidgets();
}

void VolumeFilterList::addFilter(FilterProperties* filterProperties) {
    filterList_.addItem(filterProperties->getVolumeFilterName());
    filterProperties_.push_back(std::unique_ptr<FilterProperties>(filterProperties));
    for(Property* property : filterProperties->getProperties()) {
        addProperty(property);
        property->setGroupID(filterProperties->getVolumeFilterName());
        ON_CHANGE((*property), VolumeFilterList, onFilterPropertyChange);
    }
    filterProperties->storeVisibility();
    setPropertyGroupGuiName(filterProperties->getVolumeFilterName(), filterProperties->getVolumeFilterName());
    setPropertyGroupVisible(filterProperties->getVolumeFilterName(), false);
}

}   // namespace
