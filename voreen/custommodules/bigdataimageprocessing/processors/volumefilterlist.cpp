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

#include "../volumefiltering/binarymedianfilter.h"
#include "../volumefiltering/gaussianfilter.h"
#include "../volumefiltering/medianfilter.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace voreen {

class FilterProperties : public Serializable {
public:

    static const int DEFAULT_SETTINGS;

    virtual ~FilterProperties() {}

    const std::vector<Property*> getProperties() const {
        return properties_;
    }

    void storeVisibility() {
        for(Property* property : properties_) {
            visibilityMap_[property] = property->isVisibleFlagSet();
        }
    }
    void restoreVisibility() {
        for(Property* property : properties_) {
            property->setVisibleFlag(visibilityMap_[property]);
        }
    }

    virtual std::string getVolumeFilterName() const = 0;
    virtual void adjustPropertiesToInput(const VolumeBase& input) = 0;
    virtual VolumeFilter* getVolumeFilter(const VolumeBase& volume, int instanceId) const = 0;
    virtual void storeInstance(int instanceId) = 0;
    virtual void restoreInstance(int instanceId) = 0;
    virtual void removeInstance(int instanceId) = 0;

protected:

    virtual void addProperties() = 0;

    std::string getId(const std::string& id) const {
        std::string name = getVolumeFilterName();
        boost::algorithm::replace_all(name, " ", "_");
        return name + "_" + id;
    }

    std::vector<Property*> properties_;
    std::map<Property*, bool> visibilityMap_;

    static const std::string loggerCat_;
};

const int FilterProperties::DEFAULT_SETTINGS = -1;
const std::string FilterProperties::loggerCat_ = "voreen.base.VolumeFilterList";

class BinaryMedianFilterProperties : public FilterProperties {
public:
    BinaryMedianFilterProperties()
        : extentX_(getId("extentx"), "Extent X", 1, 1, 10)
        , extentY_(getId("extenty"), "Extent Y", 1, 1, 10)
        , extentZ_(getId("extentz"), "Extent Z", 1, 1, 10)
        , binarizationThreshold_(getId("binarizationThreshold"), "Threshold", 0.5f, 0.0f, std::numeric_limits<float>::max(), Processor::INVALID_RESULT, FloatProperty::STATIC, Property::LOD_ADVANCED)
        , samplingStrategyType_(getId("samplingStrategyType"), "Sampling Strategy", SamplingStrategyType::CLAMP_T)
        , outsideVolumeValue_(getId("outsideVolumeValue"), "Outside Volume Value", 0, 0, 1)
        , forceMedian_(getId("forceMedian"), "Force Median", true)
        , objectVoxelThreshold_(getId("objectVoxelThreshold"), "Object Voxel Threshold", 0, 0, std::numeric_limits<int>::max())
    {
        samplingStrategyType_.addOption("clamp", "Clamp", SamplingStrategyType::CLAMP_T);
        samplingStrategyType_.addOption("mirror", "Mirror", SamplingStrategyType::MIRROR_T);
        samplingStrategyType_.addOption("set", "Set", SamplingStrategyType::SET_T);
        ON_CHANGE_LAMBDA(samplingStrategyType_, [this] () {
            outsideVolumeValue_.setVisibleFlag(samplingStrategyType_.getValue() == SamplingStrategyType::SET_T);
        });

        ON_CHANGE(forceMedian_, BinaryMedianFilterProperties, updateObjectVoxelThreshold);

        // Update property state.
        samplingStrategyType_.invalidate();
        updateObjectVoxelThreshold();

        // Store default settings.
        storeInstance(DEFAULT_SETTINGS);

        // Add properties to list.
        addProperties();
    }

    virtual std::string getVolumeFilterName() const {
        return "Binary Median Filter";
    }

    virtual void adjustPropertiesToInput(const VolumeBase& input) {
        if(!input.hasDerivedData<VolumeMinMax>()) {
            LINFO("Calculating VolumeMinMax. This may take a while...");
        }
        const VolumeMinMax* mm = input.getDerivedData<VolumeMinMax>();

        binarizationThreshold_.setMinValue(mm->getMin());
        binarizationThreshold_.setMaxValue(mm->getMax());
        binarizationThreshold_.adaptDecimalsToRange(2);
    }

    virtual VolumeFilter* getVolumeFilter(const VolumeBase& volume, int instanceId) const {
        if(instanceSettings_.find(instanceId) == instanceSettings_.end()) {
            return nullptr;
        }
        Settings settings = instanceSettings_.at(instanceId);
        RealWorldMapping rwm;
        if(volume.hasMetaData("RealWorldMapping")) {
            rwm = volume.getRealWorldMapping();
        }
        return new BinaryMedianFilter(
                tgt::ivec3(settings.extentX_, settings.extentY_, settings.extentZ_),
                rwm.realWorldToNormalized(settings.binarizationThreshold_),
                settings.objectVoxelThreshold_,
                SamplingStrategy<float>(settings.samplingStrategyType_, static_cast<float>(settings.outsideVolumeValue_))
        );
    }
    virtual void restoreInstance(int instanceId) {
        auto iter = instanceSettings_.find(instanceId);
        if(iter == instanceSettings_.end()) {
            instanceSettings_[instanceId] = instanceSettings_[DEFAULT_SETTINGS];
        }

        Settings settings = instanceSettings_[instanceId];
        extentX_.set(settings.extentX_);
        extentY_.set(settings.extentY_);
        extentZ_.set(settings.extentZ_);
        binarizationThreshold_.set(settings.binarizationThreshold_);
        samplingStrategyType_.selectByValue(settings.samplingStrategyType_);
        outsideVolumeValue_.set(settings.outsideVolumeValue_);
        forceMedian_.set(settings.forceMedian_);
        objectVoxelThreshold_.set(settings.objectVoxelThreshold_);
    }
    virtual void storeInstance(int instanceId) {
        Settings& settings = instanceSettings_[instanceId];
        settings.extentX_ = extentX_.get();
        settings.extentY_ = extentY_.get();
        settings.extentZ_ = extentZ_.get();
        settings.binarizationThreshold_ = binarizationThreshold_.get();
        settings.samplingStrategyType_ = samplingStrategyType_.getValue();
        settings.outsideVolumeValue_ = outsideVolumeValue_.get();
        settings.forceMedian_ = forceMedian_.get();
        settings.objectVoxelThreshold_ = objectVoxelThreshold_.get();
    }
    virtual void removeInstance(int instanceId) {
        instanceSettings_.erase(instanceId);
    }
    virtual void addProperties() {
        properties_.push_back(&extentX_);
        properties_.push_back(&extentY_);
        properties_.push_back(&extentZ_);
        properties_.push_back(&binarizationThreshold_);
        properties_.push_back(&samplingStrategyType_);
        properties_.push_back(&outsideVolumeValue_);
        properties_.push_back(&forceMedian_);
        properties_.push_back(&objectVoxelThreshold_);
    }
    virtual void serialize(Serializer& s) const {
        std::vector<int> names;
        std::vector<Settings> settings;
        for(const auto& pair : instanceSettings_) {
            names.push_back(pair.first);
            settings.push_back(pair.second);
        }
        s.serializeBinaryBlob(getId("names"), names);
        s.serializeBinaryBlob(getId("settings"), settings);
    }
    virtual void deserialize(Deserializer& s) {
        std::vector<int> names;
        std::vector<Settings> settings;
        s.deserializeBinaryBlob(getId("names"), names);
        s.deserializeBinaryBlob(getId("settings"), settings);
        tgtAssert(names.size() == settings.size(), "number of keys and values does not match");
        for(size_t i = 0; i < names.size(); i++) {
            instanceSettings_[names[i]] = settings[i];
        }
    }

private:

    void updateObjectVoxelThreshold() {
        bool medianForced = forceMedian_.get();
        objectVoxelThreshold_.setReadOnlyFlag(medianForced);
        objectVoxelThreshold_.setMaxValue((2*extentX_.get()+1)*(2*extentY_.get()+1)*(2*extentZ_.get()+1));
        if(medianForced) {
            objectVoxelThreshold_.set(objectVoxelThreshold_.getMaxValue()/2);
        }
    }

    struct Settings {
        int extentX_;
        int extentY_;
        int extentZ_;
        float binarizationThreshold_;
        SamplingStrategyType samplingStrategyType_;
        int outsideVolumeValue_;
        bool forceMedian_;
        int objectVoxelThreshold_;
    };
    std::map<int, Settings> instanceSettings_;

    IntProperty extentX_;
    IntProperty extentY_;
    IntProperty extentZ_;
    FloatProperty binarizationThreshold_;
    OptionProperty<SamplingStrategyType> samplingStrategyType_;
    IntProperty outsideVolumeValue_;
    BoolProperty forceMedian_;
    IntProperty objectVoxelThreshold_;
};

class MedianFilterProperties : public FilterProperties {
public:
    MedianFilterProperties()
            : extent_(getId("extent"), "Extent", 1, 1, 10)
            , samplingStrategyType_(getId("samplingStrategyType"), "Sampling Strategy", SamplingStrategyType::CLAMP_T)
            , outsideVolumeValue_(getId("outsideVolumeValue"), "Outside Volume Value", 0, 0, 1)
    {
        samplingStrategyType_.addOption("clamp", "Clamp", SamplingStrategyType::CLAMP_T);
        samplingStrategyType_.addOption("mirror", "Mirror", SamplingStrategyType::MIRROR_T);
        samplingStrategyType_.addOption("set", "Set", SamplingStrategyType::SET_T);
        ON_CHANGE_LAMBDA(samplingStrategyType_, [this] () {
            outsideVolumeValue_.setVisibleFlag(samplingStrategyType_.getValue() == SamplingStrategyType::SET_T);
        });

        // Update property state.
        samplingStrategyType_.invalidate();

        // Store default settings.
        storeInstance(DEFAULT_SETTINGS);

        // Add properties to list.
        addProperties();
    }

    virtual std::string getVolumeFilterName() const {
        return "Median Filter";
    }

    virtual void adjustPropertiesToInput(const VolumeBase& input) {
        // unused
    }

    virtual VolumeFilter* getVolumeFilter(const VolumeBase& volume, int instanceId) const {
        if(instanceSettings_.find(instanceId) == instanceSettings_.end()) {
            return nullptr;
        }
        Settings settings = instanceSettings_.at(instanceId);
        return new MedianFilter(
                settings.extent_,
                SamplingStrategy<float>(settings.samplingStrategyType_, static_cast<float>(settings.outsideVolumeValue_)),
                volume.getBaseType()
        );
    }
    virtual void restoreInstance(int instanceId) {
        auto iter = instanceSettings_.find(instanceId);
        if(iter == instanceSettings_.end()) {
            instanceSettings_[instanceId] = instanceSettings_[DEFAULT_SETTINGS];
        }

        Settings settings = instanceSettings_[instanceId];
        extent_.set(settings.extent_);
        samplingStrategyType_.selectByValue(settings.samplingStrategyType_);
        outsideVolumeValue_.set(settings.outsideVolumeValue_);
    }
    virtual void storeInstance(int instanceId) {
        Settings& settings = instanceSettings_[instanceId];
        settings.extent_ = extent_.get();
        settings.samplingStrategyType_ = samplingStrategyType_.getValue();
        settings.outsideVolumeValue_ = outsideVolumeValue_.get();
    }
    virtual void removeInstance(int instanceId) {
        instanceSettings_.erase(instanceId);
    }
    virtual void addProperties() {
        properties_.push_back(&extent_);
        properties_.push_back(&samplingStrategyType_);
        properties_.push_back(&outsideVolumeValue_);
    }
    virtual void serialize(Serializer& s) const {
        std::vector<int> names;
        std::vector<Settings> settings;
        for(const auto& pair : instanceSettings_) {
            names.push_back(pair.first);
            settings.push_back(pair.second);
        }
        s.serializeBinaryBlob(getId("names"), names);
        s.serializeBinaryBlob(getId("settings"), settings);
    }
    virtual void deserialize(Deserializer& s) {
        std::vector<int> names;
        std::vector<Settings> settings;
        s.deserializeBinaryBlob(getId("names"), names);
        s.deserializeBinaryBlob(getId("settings"), settings);
        tgtAssert(names.size() == settings.size(), "number of keys and values does not match");
        for(size_t i = 0; i < names.size(); i++) {
            instanceSettings_[names[i]] = settings[i];
        }
    }

private:

    struct Settings {
        int extent_;
        SamplingStrategyType samplingStrategyType_;
        int outsideVolumeValue_;
    };
    std::map<int, Settings> instanceSettings_;

    IntProperty extent_;
    IntProperty outsideVolumeValue_;
    OptionProperty<SamplingStrategyType> samplingStrategyType_;
};

class GaussianFilterProperties : public FilterProperties {
public:
    GaussianFilterProperties()
        : extentX_(getId("extentx"), "Extent X", 1, 1, 10)
        , extentY_(getId("extenty"), "Extent Y", 1, 1, 10)
        , extentZ_(getId("extentz"), "Extent Z", 1, 1, 10)
        , samplingStrategyType_(getId("samplingStrategyType"), "Sampling Strategy", SamplingStrategyType::CLAMP_T)
        , outsideVolumeValue_(getId("outsideVolumeValue"), "Outside Volume Value", 0, 0, 1)
    {
        samplingStrategyType_.addOption("clamp", "Clamp", SamplingStrategyType::CLAMP_T);
        samplingStrategyType_.addOption("mirror", "Mirror", SamplingStrategyType::MIRROR_T);
        samplingStrategyType_.addOption("set", "Set", SamplingStrategyType::SET_T);

        // Update property state.
        samplingStrategyType_.invalidate();

        // Store default settings.
        storeInstance(DEFAULT_SETTINGS);

        // Add properties to list.
        addProperties();
    }

    virtual std::string getVolumeFilterName() const {
        return "Gaussian Filter";
    }

    virtual void adjustPropertiesToInput(const VolumeBase&) {
        // unused
    }

    virtual VolumeFilter* getVolumeFilter(const VolumeBase& volume, int instanceId) const {
        if(instanceSettings_.find(instanceId) == instanceSettings_.end()) {
            return nullptr;
        }
        Settings settings = instanceSettings_.at(instanceId);
        return new GaussianFilter(
                tgt::ivec3(settings.extentX_, settings.extentY_, settings.extentZ_),
                SamplingStrategy<float>(settings.samplingStrategyType_, static_cast<float>(settings.outsideVolumeValue_)),
                volume.getBaseType()
        );
    }
    virtual void restoreInstance(int instanceId) {
        auto iter = instanceSettings_.find(instanceId);
        if(iter == instanceSettings_.end()) {
            instanceSettings_[instanceId] = instanceSettings_[DEFAULT_SETTINGS];
        }

        Settings settings = instanceSettings_[instanceId];
        extentX_.set(settings.extentX_);
        extentY_.set(settings.extentY_);
        extentZ_.set(settings.extentZ_);
        samplingStrategyType_.selectByValue(settings.samplingStrategyType_);
        outsideVolumeValue_.set(settings.outsideVolumeValue_);
    }
    virtual void storeInstance(int instanceId) {
        Settings& settings = instanceSettings_[instanceId];
        settings.extentX_ = extentX_.get();
        settings.extentY_ = extentY_.get();
        settings.extentZ_ = extentZ_.get();
        settings.samplingStrategyType_ = samplingStrategyType_.getValue();
        settings.outsideVolumeValue_ = outsideVolumeValue_.get();
    }
    virtual void removeInstance(int instanceId) {
        instanceSettings_.erase(instanceId);
    }
    virtual void addProperties() {
        properties_.push_back(&extentX_);
        properties_.push_back(&extentY_);
        properties_.push_back(&extentZ_);
        properties_.push_back(&samplingStrategyType_);
        properties_.push_back(&outsideVolumeValue_);
    }
    virtual void serialize(Serializer& s) const {
        std::vector<int> names;
        std::vector<Settings> settings;
        for(const auto& pair : instanceSettings_) {
            names.push_back(pair.first);
            settings.push_back(pair.second);
        }
        s.serializeBinaryBlob(getId("names"), names);
        s.serializeBinaryBlob(getId("settings"), settings);
    }
    virtual void deserialize(Deserializer& s) {
        std::vector<int> names;
        std::vector<Settings> settings;
        s.deserializeBinaryBlob(getId("names"), names);
        s.deserializeBinaryBlob(getId("settings"), settings);
        tgtAssert(names.size() == settings.size(), "number of keys and values does not match");
        for(size_t i = 0; i < names.size(); i++) {
            instanceSettings_[names[i]] = settings[i];
        }
    }

private:

    struct Settings {
        int extentX_;
        int extentY_;
        int extentZ_;
        SamplingStrategyType samplingStrategyType_;
        int outsideVolumeValue_;
    };
    std::map<int, Settings> instanceSettings_;

    IntProperty extentX_;
    IntProperty extentY_;
    IntProperty extentZ_;
    OptionProperty<SamplingStrategyType> samplingStrategyType_;
    IntProperty outsideVolumeValue_;
};



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
{
    addPort(inport_);
        ON_CHANGE(inport_, VolumeFilterList, adjustPropertiesToInput);
    addPort(outport_);

    addProperty(filterList_);
        filterList_.setGroupID("filter");
        filterList_.setDuplicationAllowed(true);
        ON_CHANGE(filterList_, VolumeFilterList, onFilterListChange);
    setPropertyGroupGuiName("filter", "Filter");

    // Add filters (this will add their properties!)
    addFilter(new BinaryMedianFilterProperties());
    addFilter(new MedianFilterProperties());
    addFilter(new GaussianFilterProperties());

    // Technical stuff.
    addProperty(enabled_);
        enabled_.setGroupID("output");
    addProperty(outputVolumeFilePath_);
        outputVolumeFilePath_.setGroupID("output");
    addProperty(outputVolumeDeflateLevel_);
        outputVolumeDeflateLevel_.setGroupID("output");
    setPropertyGroupGuiName("output", "Output");
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
        filterProperties_[i]->deserialize(s);
    }
}

VolumeFilterListInput VolumeFilterList::prepareComputeInput() {
    if(!enabled_.get()) {
        return VolumeFilterListInput(
                nullptr,
                nullptr
        );
    }

    if(!inport_.hasData()) {
        throw InvalidInputException("No input", InvalidInputException::S_WARNING);
    }

    if(filterList_.getInstances().empty()) {
        throw InvalidInputException("No filter selected", InvalidInputException::S_ERROR);
    }

    auto inputVolPtr = inport_.getThreadSafeData();
    const VolumeBase& inputVolume = *inputVolPtr;

    if(inputVolume.getNumChannels() != 1) {
        throw InvalidInputException("Input volume has multiple channels, but a single channel volume is expected!", InvalidInputException::S_ERROR);
    }

    // Reset output volume to make sure it (and the hdf5filevolume) are not used any more
    outport_.setData(nullptr);

    const std::string volumeFilePath = outputVolumeFilePath_.get();
    const std::string volumeLocation = HDF5VolumeWriter::VOLUME_DATASET_NAME;
    const std::string baseType = "uint8";
    const tgt::svec3 dim = inputVolume.getDimensions();

    if(volumeFilePath.empty()) {
        throw InvalidInputException("No volume file path specified!", InvalidInputException::S_ERROR);
    }

    std::unique_ptr<HDF5FileVolume> outputVolume = nullptr;
    try {
        outputVolume = std::unique_ptr<HDF5FileVolume>(HDF5FileVolume::createVolume(volumeFilePath, volumeLocation, baseType, dim, 1, true, outputVolumeDeflateLevel_.get(), tgt::svec3(dim.x, dim.y, 1), false));
    } catch(tgt::IOException e) {
        throw InvalidInputException("Could not create output volume.", InvalidInputException::S_ERROR);
    }

    outputVolume->writeSpacing(inputVolume.getSpacing());
    outputVolume->writeOffset(inputVolume.getOffset());
    outputVolume->writeRealWorldMapping(RealWorldMapping(1,0,""));
    // For all zero or all one volumes the following is not correct,
    // and we cannot easily get the real min/max values without iterating
    // through the whole resulting volume.
    //const VolumeMinMax vmm(0, 1, 0, 1);
    //outputVolume->writeVolumeMinMax(&vmm);

    VolumeFilterStackBuilder builder(inputVolume);
    for(const InteractiveListProperty::Instance& instance : filterList_.getInstances()) {
        VolumeFilter* filter = filterProperties_[instance.itemId_]->getVolumeFilter(inputVolume, instance.instanceId_);
        if(!filter) {
            LWARNING("Filter: '" << filterList_.getInstanceName(instance)
                                 << "' has not been configured yet. Taking default.");
            filter = filterProperties_[instance.itemId_]->getVolumeFilter(inputVolume,
                                                                          FilterProperties::DEFAULT_SETTINGS);
        }
        builder.addLayer(std::unique_ptr<VolumeFilter>(filter));
    }

    std::unique_ptr<SliceReader> sliceReader = builder.build(0);

    return VolumeFilterListInput(
            std::move(sliceReader),
            std::move(outputVolume)
    );
}
VolumeFilterListOutput VolumeFilterList::compute(VolumeFilterListInput input, ProgressReporter& progressReporter) const {
    if(!enabled_.get()) {
        return { "" };
    }
    tgtAssert(input.sliceReader, "No sliceReader");
    tgtAssert(input.outputVolume, "No outputVolume");

    writeSlicesToHDF5File(*input.sliceReader, *input.outputVolume, &progressReporter);

    return {
        input.outputVolume->getFileName()
    };
    //outputVolume will be destroyed and thus closed now.
}
void VolumeFilterList::processComputeOutput(VolumeFilterListOutput output) {
    if(!enabled_.get()) {
        outport_.setData(inport_.getData(), false);
    } else {
        // outputVolume has been destroyed and thus closed by now.
        // So we can open it again (and use HDF5VolumeReader's implementation to read all the metadata with the file)
        const VolumeBase* vol = HDF5VolumeReader().read(output.outputVolumeFilePath)->at(0);
        outport_.setData(vol);
    }
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
            setPropertyGroupVisible(filterList_.getItems()[selectedInstance_->itemId_], false);
            filterProperties_[selectedInstance_->itemId_]->removeInstance(selectedInstance_->instanceId_);
            selectedInstance_.reset();
        }
        numInstances_ = filterList_.getInstances().size();
    }

    // Hide old group.
    if(selectedInstance_) {
        filterProperties_[selectedInstance_->itemId_]->storeVisibility();
        // No need to store the settings here, since it is done on change anyways.
        //filterProperties_[selectedInstance_->itemId_]->storeInstance(selectedInstance_->instanceId_);
        setPropertyGroupVisible(filterList_.getItems()[selectedInstance_->itemId_], false);

        // We need to reset here, because otherwise onFilterPropertyChange
        // will be triggered while the current instance is restored.
        selectedInstance_.reset();
    }

    // Show new group.
    boost::optional<InteractiveListProperty::Instance> currentInstance;
    if(filterList_.getSelectedInstance() != -1) {
        currentInstance = filterList_.getInstances()[filterList_.getSelectedInstance()];
        setPropertyGroupVisible(filterList_.getItems()[currentInstance->itemId_], true);
        filterProperties_[currentInstance->itemId_]->restoreVisibility();
        filterProperties_[currentInstance->itemId_]->restoreInstance(currentInstance->instanceId_);
    }

    selectedInstance_ = currentInstance;
}

void VolumeFilterList::onFilterPropertyChange() {
    // If any filter property was modified, we need to store the settings immediately.
    if(selectedInstance_) {
        filterProperties_[selectedInstance_->itemId_]->storeInstance(selectedInstance_->instanceId_);
    }
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
