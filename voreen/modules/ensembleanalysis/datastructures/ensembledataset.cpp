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

#include "ensembledataset.h"

#include "voreen/core/datastructures/volume/volumedisk.h"
#include "voreen/core/datastructures/volume/volumeminmax.h"
#include "voreen/core/datastructures/volume/volumeminmaxmagnitude.h"

#include "../utils/utils.h"

namespace voreen {


///////////////////////////////////////////////////
// class TimeStep::VolumeCache
///////////////////////////////////////////////////

VolumeURL TimeStep::VolumeCache::getOrConstructURL(std::pair<std::string, const VolumeBase*> volume) {
    // In case the Volume URL is empty, the volume most likely only is present in RAM.
    // Since we still require a URL to identify the volume, we simply construct a unique URL.
    VolumeURL url = volume.second->getOrigin();
    if(url == VolumeURL()) {
        url = VolumeURL("RAM", volume.first);
    }
    return url;
}

TimeStep::VolumeCache::VolumeCache()
{
}

TimeStep::VolumeCache::VolumeCache(const std::map<std::string, const VolumeBase*>& volumeData) {
    for(const auto& vol : volumeData) {
        const VolumeBase* volume = vol.second;
        volume->Observable<VolumeObserver>::addObserver(static_cast<const VolumeObserver*>(this));
        cacheEntries_.insert(std::make_pair(getOrConstructURL(vol).getURL(), VolumeCacheEntry{volume, false}));
    }
}

TimeStep::VolumeCache::~VolumeCache() {
    for(const auto& entry : cacheEntries_) {
        if(entry.second.owned_) {
            delete entry.second.volume_;
        }
    }
}

void TimeStep::VolumeCache::volumeDelete(const VolumeBase* source) {
    boost::lock_guard<boost::mutex> lockGuard(volumeDataMutex_);

    // If the volume gets deleted by the owner, we remove it from the cache.
    // Otherwise, we would have a dangling pointer.
    for(auto iter = cacheEntries_.begin(); iter != cacheEntries_.end(); iter++) {
        if(iter->second.volume_ == source) {
            cacheEntries_.erase(iter);
            return;
        }
    }
}

void TimeStep::VolumeCache::volumeChange(const VolumeBase* source) {
}

bool TimeStep::VolumeCache::isOwned(const VolumeURL& url) {
    boost::lock_guard<boost::mutex> lockGuard(volumeDataMutex_);
    auto volIter = cacheEntries_.find(url.getURL());
    if(volIter != cacheEntries_.end()) {
        return volIter->second.owned_;
    }
    return false;
}

const VolumeBase* TimeStep::VolumeCache::requestVolume(const VolumeURL& url) {
    boost::lock_guard<boost::mutex> lockGuard(volumeDataMutex_);

    // First query volume data.
    std::string urlString = url.getURL();
    auto volIter = cacheEntries_.find(urlString);
    if(volIter != cacheEntries_.end()) {
        return volIter->second.volume_;
    }

    // If not available, load it using the stored url.
    VolumeBase* volume = EnsembleVolumeReader().read(url);
    if(!volume) {
        return nullptr;
    }

    // Cache result.
    cacheEntries_.insert(std::make_pair(urlString, VolumeCacheEntry{volume, true}));

    return volume;
}

///////////////////////////////////////////////////
// class TimeStep::DerivedData
///////////////////////////////////////////////////

TimeStep::DerivedData::DerivedData()
{
}

TimeStep::DerivedData::DerivedData(const VolumeBase* volume, bool calculateIfNotPresent) {
    if(volume->hasDerivedData<VolumeMinMax>() || calculateIfNotPresent) {
        minMax_ = *volume->getDerivedData<VolumeMinMax>();
    }
    if(volume->hasDerivedData<VolumeMinMaxMagnitude>() || (calculateIfNotPresent && volume->getNumChannels() > 1)) {
        minMaxMagnitude_ = *volume->getDerivedData<VolumeMinMaxMagnitude>();
    }
}

void TimeStep::DerivedData::addToVolume(VolumeBase* volume) {
    if(minMax_) {
        volume->addDerivedData(new VolumeMinMax(minMax_.get()));
    }
    if(minMaxMagnitude_) {
        volume->addDerivedData(new VolumeMinMaxMagnitude(minMaxMagnitude_.get()));
    }
}

void TimeStep::DerivedData::serialize(Serializer& s) const {
    if(minMax_) {
        s.serialize("minMax", minMax_.get());
    }
    if(minMaxMagnitude_) {
        s.serialize("minMaxMagnitude", minMaxMagnitude_.get());
    }
}
void TimeStep::DerivedData::deserialize(Deserializer& s) {
    try {
        VolumeMinMax minMax;
        s.deserialize("minMax", minMax);
        minMax_ = minMax;
    } catch (SerializationException&) {
        s.removeLastError();
        minMax_ = boost::none;
    }

    try {
        VolumeMinMaxMagnitude minMaxMagnitude;
        s.deserialize("minMaxMagnitude", minMaxMagnitude);
        minMaxMagnitude_ = minMaxMagnitude;
    } catch (SerializationException&) {
        s.removeLastError();
        minMaxMagnitude_ = boost::none;
    }
}

///////////////////////////////////////////////////
// class TimeStep
///////////////////////////////////////////////////

TimeStep::TimeStep()
    : TimeStep(std::map<std::string, const VolumeBase*>(), 0.0f)
{}

TimeStep::TimeStep(const std::map<std::string, const VolumeBase*>& volumeData, float time, bool enforceDerivedData)
    : time_(time)
    , volumeCache_(new VolumeCache(volumeData))
{
    for(const auto& vol : volumeData) {
        std::string fieldName = vol.first;
        urls_.insert(std::make_pair(fieldName, VolumeCache::getOrConstructURL(vol)));
        derivedData_.insert(std::make_pair(fieldName, DerivedData(vol.second, enforceDerivedData)));
    }
}

TimeStep TimeStep::createSubset(const std::vector<std::string>& fieldNames) const {
    TimeStep subset = *this;

    // Reset URLs.
    auto& urls = subset.urls_;
    urls.clear();

    // Add back requested URLs.
    for(const std::string& fieldName : fieldNames) {
        auto iter = urls_.find(fieldName);
        if(iter != urls_.end()) {
            urls.insert(*iter);
        }
    }

    return subset;
}

float TimeStep::getTime() const {
    return time_;
}

float TimeStep::operator-(const TimeStep& rhs) const {
    return getTime() - rhs.getTime();
}

bool TimeStep::operator<(const TimeStep& rhs) const {
    return getTime() < rhs.getTime();
}
bool TimeStep::operator<=(const TimeStep& rhs) const {
    return getTime() <= rhs.getTime();
}

bool TimeStep::operator>(const TimeStep& rhs) const {
    return getTime() > rhs.getTime();
}
bool TimeStep::operator>=(const TimeStep& rhs) const {
    return getTime() >= rhs.getTime();
}

bool TimeStep::operator==(const TimeStep& rhs) const {
    return getTime() == rhs.getTime();
}
bool TimeStep::operator!=(const TimeStep& rhs) const {
    return getTime() != rhs.getTime();
}

std::vector<std::string> TimeStep::getFieldNames() const {
    std::vector<std::string> fieldNames;
    for(const auto& volumeData : urls_) {
        fieldNames.push_back(volumeData.first);
    }
    return fieldNames;
}

const VolumeBase* TimeStep::getVolume(const std::string& fieldName) const {
    auto urlIter = urls_.find(fieldName);
    if(urlIter != urls_.end()) {
        const VolumeURL& url = urlIter->second;
        const VolumeBase* volume = volumeCache_->requestVolume(url);

        // Add back derived data, if the volume was loaded lazily.
        if(volume && volumeCache_->isOwned(url)) {
            auto derivedDataIter = derivedData_.find(fieldName);
            if (derivedDataIter != derivedData_.end()) {
                // As the cache owns the volume, we can safely apply const cast.
                // TODO: This is far from ideal. The volume cache should add the meta data directly,
                //  however it is not serialized. Hence, the cache needs to know the derived data.
                //  We should try to avoid the cache in the first place..
                //  Additionally, VolumeRAMSwap does currently not add back meta data, which it definitely should!
                derivedDataIter->second.addToVolume(const_cast<VolumeBase*>(volume));
            }
        }

        return volume;
    }

    return nullptr;
}

VolumeURL TimeStep::getURL(const std::string& fieldName) const {
    auto iter = urls_.find(fieldName);
    if(iter != urls_.end()) {
        return iter->first;
    }

    return VolumeURL();
}

void TimeStep::serialize(Serializer& s) const {
    s.serialize("time", time_);
    s.serialize("urls", urls_);
    s.serialize("derivedData", derivedData_);
}

void TimeStep::deserialize(Deserializer& s) {
    s.deserialize("time", time_);
    s.deserialize("urls", urls_);
    s.optionalDeserialize("derivedData", derivedData_, std::map<std::string, DerivedData>());

    volumeCache_.reset(new VolumeCache());
}


///////////////////////////////////////////////////
// class Member
///////////////////////////////////////////////////

EnsembleMember::EnsembleMember()
    : EnsembleMember("", tgt::vec3::zero, std::vector<TimeStep>())
{}

EnsembleMember::EnsembleMember(const std::string& name, const tgt::vec3& color, const std::vector<TimeStep>& timeSteps)
    : name_(name)
    , color_(color)
    , timeSteps_(timeSteps)
    , timeStepDurationStats_(false)
{
}

const std::string& EnsembleMember::getName() const {
    return name_;
}

const tgt::vec3& EnsembleMember::getColor() const {
    return color_;
}

const std::vector<TimeStep>& EnsembleMember::getTimeSteps() const {
    return timeSteps_;
}

size_t EnsembleMember::getTimeStep(float time) const {
    if(timeSteps_.empty())
        return -1;

    size_t t = 0;
    while (t < getTimeSteps().size()-1 && getTimeSteps()[t].getTime() < time) t++;
    return t;
}

const Statistics& EnsembleMember::getTimeStepDurationStats() const {
    if(timeStepDurationStats_.getNumSamples() == 0 && !timeSteps_.empty()) {
        float last = timeSteps_.front().getTime();
        for(size_t i=1; i<timeSteps_.size(); i++) {
            float duration = timeSteps_[i].getTime() - last;
            timeStepDurationStats_.addSample(duration);
            last = timeSteps_[i].getTime();
        }
    }
    return timeStepDurationStats_;
}

void EnsembleMember::serialize(Serializer& s) const {
    s.serialize("name", name_);
    s.serialize("color", color_);
    s.serialize("timeSteps", timeSteps_);
}

void EnsembleMember::deserialize(Deserializer& s) {
    s.deserialize("name", name_);
    s.deserialize("color", color_);
    s.deserialize("timeSteps", timeSteps_);
}

//////////// Field

EnsembleFieldMetaData::EnsembleFieldMetaData()
    : valueRange_(tgt::vec2::zero)
    , magnitudeRange_(tgt::vec2::zero)
    , numChannels_(0)
    , dimensions_(tgt::svec3::zero)
{}

bool EnsembleFieldMetaData::hasHomogeneousDimensions() const {
    return dimensions_ != tgt::svec3::zero;
}

void EnsembleFieldMetaData::serialize(Serializer& s) const {
    s.serialize("valueRange", valueRange_);
    s.serialize("magnitudeRange", magnitudeRange_);
    s.serialize("numChannels", numChannels_);
    s.serialize("dimensions", dimensions_);
}

void EnsembleFieldMetaData::deserialize(Deserializer& s) {
    s.deserialize("valueRange", valueRange_);
    s.deserialize("magnitudeRange", magnitudeRange_);
    s.deserialize("numChannels", numChannels_);
    s.optionalDeserialize("dimensions", dimensions_, tgt::svec3::zero);
}

//////////// EnsembleDataset

const std::string EnsembleDataset::loggerCat_ = "voreen.ensembleanalysis.EnsembleDataSet";

EnsembleDataset::EnsembleDataset()
    : minNumTimeSteps_(std::numeric_limits<size_t>::max())
    , maxNumTimeSteps_(0)
    , totalNumTimeSteps_(0)
    , maxTimeStepDuration_(0.0f)
    , minTimeStepDuration_(std::numeric_limits<float>::max())
    , startTime_(std::numeric_limits<float>::max())
    , endTime_(std::numeric_limits<float>::lowest())
    , commonTimeInterval_(endTime_, startTime_)
    , bounds_()
    , commonBounds_()
{
}

EnsembleDataset::EnsembleDataset(const EnsembleDataset& origin)
    : members_(origin.members_)
    , uniqueFieldNames_(origin.uniqueFieldNames_)
    , commonFieldNames_(origin.commonFieldNames_)
    , fieldMetaData_(origin.fieldMetaData_)
    , allParameters_(origin.allParameters_)
    , minNumTimeSteps_(origin.minNumTimeSteps_)
    , maxNumTimeSteps_(origin.maxNumTimeSteps_)
    , totalNumTimeSteps_(origin.totalNumTimeSteps_)
    , maxTimeStepDuration_(origin.maxTimeStepDuration_)
    , minTimeStepDuration_(origin.minTimeStepDuration_)
    , startTime_(origin.startTime_)
    , endTime_(origin.endTime_)
    , commonTimeInterval_(origin.commonTimeInterval_)
    , bounds_(origin.bounds_)
    , commonBounds_(origin.commonBounds_)
{
}

EnsembleDataset::EnsembleDataset(EnsembleDataset&& origin)
    : members_(std::move(origin.members_))
    , uniqueFieldNames_(std::move(origin.uniqueFieldNames_))
    , commonFieldNames_(std::move(origin.commonFieldNames_))
    , fieldMetaData_(std::move(origin.fieldMetaData_))
    , allParameters_(std::move(origin.allParameters_))
    , minNumTimeSteps_(origin.minNumTimeSteps_)
    , maxNumTimeSteps_(origin.maxNumTimeSteps_)
    , totalNumTimeSteps_(origin.totalNumTimeSteps_)
    , maxTimeStepDuration_(origin.maxTimeStepDuration_)
    , minTimeStepDuration_(origin.minTimeStepDuration_)
    , startTime_(origin.startTime_)
    , endTime_(origin.endTime_)
    , commonTimeInterval_(origin.commonTimeInterval_)
    , bounds_(origin.bounds_)
    , commonBounds_(origin.commonBounds_)
{
}

EnsembleDataset& EnsembleDataset::operator=(const EnsembleDataset& origin) {

    if(&origin == this) {
        return *this;
    }

    members_                    = origin.members_;
    uniqueFieldNames_           = origin.uniqueFieldNames_;
    commonFieldNames_           = origin.commonFieldNames_;
    fieldMetaData_              = origin.fieldMetaData_;
    allParameters_              = origin.allParameters_;
    minNumTimeSteps_            = origin.minNumTimeSteps_;
    maxNumTimeSteps_            = origin.maxNumTimeSteps_;
    totalNumTimeSteps_          = origin.totalNumTimeSteps_;
    maxTimeStepDuration_        = origin.maxTimeStepDuration_;
    minTimeStepDuration_        = origin.minTimeStepDuration_;
    startTime_                  = origin.startTime_;
    endTime_                    = origin.endTime_;
    commonTimeInterval_         = origin.commonTimeInterval_;
    bounds_                     = origin.bounds_;
    commonBounds_               = origin.commonBounds_;

    return *this;
}

EnsembleDataset& EnsembleDataset::operator=(EnsembleDataset&& origin) {
    this->~EnsembleDataset();
    new(this) EnsembleDataset(std::move(origin));
    return *this;
}

void EnsembleDataset::addMember(const EnsembleMember& member) {

    // Skip empty members.
    if (member.getTimeSteps().empty()) {
        LERROR("Can't add empty member");
        return;
    }

    // Notify Observers
    notifyPendingDataInvalidation();

    minNumTimeSteps_ = std::min(member.getTimeSteps().size(), minNumTimeSteps_);
    maxNumTimeSteps_ = std::max(member.getTimeSteps().size(), maxNumTimeSteps_);
    totalNumTimeSteps_ += member.getTimeSteps().size();
    startTime_ = std::min(startTime_, member.getTimeSteps().front().getTime());
    endTime_   = std::max(endTime_,   member.getTimeSteps().back().getTime());

    for (size_t t = 0; t < member.getTimeSteps().size(); t++) {
        std::vector<std::string> fields;
        for (const std::string& fieldName : member.getTimeSteps()[t].getFieldNames()) {

            fields.push_back(fieldName);

            // Retrieve volume.
            const VolumeBase* volume = member.getTimeSteps()[t].getVolume(fieldName);

            // Gather parameters (take first time step as representative).
            if(t==0) {
                for (const std::string& key : volume->getMetaDataKeys()) {
                    if (key.find("Parameter") != std::string::npos) {
                        allParameters_.insert(key);
                    }
                }
            }

            // Gather derived data.
            VolumeMinMax* vmm = volume->getDerivedData<VolumeMinMax>();
            tgt::vec2 minMax(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
            for(size_t c = 0; c < vmm->getNumChannels(); c++) {
                minMax.x = std::min(minMax.x, vmm->getMin(c));
                minMax.y = std::max(minMax.y, vmm->getMax(c));
            }

            tgt::vec2 minMaxMagnitude = minMax;
            if(volume->getNumChannels() > 1) {
                VolumeMinMaxMagnitude* vmmm = volume->getDerivedData<VolumeMinMaxMagnitude>();
                minMaxMagnitude.x = vmmm->getMinMagnitude();
                minMaxMagnitude.y = vmmm->getMaxMagnitude();
            }

            bool firstFieldElement = fieldMetaData_.find(fieldName) == fieldMetaData_.end();
            EnsembleFieldMetaData& fieldMetaData = fieldMetaData_[fieldName];
            if(firstFieldElement) {
                fieldMetaData.valueRange_ = minMax;
                fieldMetaData.magnitudeRange_ = minMaxMagnitude;
                fieldMetaData.numChannels_ = volume->getNumChannels();
                fieldMetaData.dimensions_ = volume->getDimensions();
            }
            else {
                fieldMetaData.valueRange_.x = std::min(fieldMetaData.valueRange_.x, minMax.x);
                fieldMetaData.valueRange_.y = std::max(fieldMetaData.valueRange_.y, minMax.y);
                fieldMetaData.magnitudeRange_.x = std::min(fieldMetaData.magnitudeRange_.x, minMaxMagnitude.x);
                fieldMetaData.magnitudeRange_.y = std::max(fieldMetaData.magnitudeRange_.y, minMaxMagnitude.y);
                if(fieldMetaData.numChannels_ != volume->getNumChannels()) {
                    LERRORC("voreen.EnsembleDataSet", "Number of channels differs per field, taking min.");
                    fieldMetaData.numChannels_ = std::min(fieldMetaData.numChannels_, volume->getNumChannels());
                }
                if(fieldMetaData.hasHomogeneousDimensions() && volume->getDimensions() != fieldMetaData.dimensions_) {
                    fieldMetaData.dimensions_ = tgt::svec3::zero;
                }
            }

            tgt::Bounds bounds = volume->getBoundingBox().getBoundingBox();
            if (!bounds_.isDefined()) {
                if(!commonBounds_.isDefined()) {
                    commonBounds_.addVolume(bounds);
                }
            }
            else if(commonBounds_.isDefined()) {
                commonBounds_.intersectVolume(bounds);
                if(!commonBounds_.isDefined()) {
                    LWARNINGC("voreen.EnsembeDataSet", "There is no overlap between the bounds of Member " << member.getName() << " and the previously defined bounds");
                }
            }
            bounds_.addVolume(bounds);
        }

        // Calculate common fields.
        if (!commonFieldNames_.empty()) {
            std::vector<std::string> intersection;
            std::set_intersection(
                commonFieldNames_.begin(),
                commonFieldNames_.end(),
                fields.begin(),
                fields.end(),
                std::back_inserter(intersection)
            );

            if (commonFieldNames_.size() != intersection.size() && !members_.empty()) {
                LWARNINGC("voreen.EnsembeDataSet", "Time Step " << t << " of Member " << member.getName() << " has less fields than the previously added Member " << members_.back().getName());
            }

            commonFieldNames_ = intersection;
        }
        else if (members_.empty()) {
            commonFieldNames_ = fields;
        }

        // Update all fields.
        std::vector<std::string> fieldUnion;
        std::set_union(uniqueFieldNames_.begin(), uniqueFieldNames_.end(), fields.begin(), fields.end(), std::back_inserter(fieldUnion));
        uniqueFieldNames_ = fieldUnion;

        // Calculate times and durations.
        if (t < member.getTimeSteps().size() - 1) {
            float duration = member.getTimeSteps()[t+1] - member.getTimeSteps()[t];
            maxTimeStepDuration_ = std::max(maxTimeStepDuration_, duration);
            minTimeStepDuration_ = std::min(minTimeStepDuration_, duration);
        }
    }

    commonTimeInterval_.x = std::max(commonTimeInterval_.x, member.getTimeSteps().front().getTime());
    commonTimeInterval_.y = std::min(commonTimeInterval_.y, member.getTimeSteps().back().getTime());

    if(commonTimeInterval_ != tgt::vec2::zero && commonTimeInterval_.x > commonTimeInterval_.y) {
        LWARNINGC("voreen.EnsembleDataSet", "The time interval of the currently added Member " << member.getName() << " does not overlap with the previous interval");
        commonTimeInterval_ = tgt::vec2::zero;
    }

    members_.push_back(member);
}

const std::vector<EnsembleMember>& EnsembleDataset::getMembers() const {
    return members_;
}

size_t EnsembleDataset::getMinNumTimeSteps() const {
    return minNumTimeSteps_;
}

size_t EnsembleDataset::getMaxNumTimeSteps() const {
    return maxNumTimeSteps_;
}

size_t EnsembleDataset::getTotalNumTimeSteps() const {
    return totalNumTimeSteps_;
}

float EnsembleDataset::getMinTimeStepDuration() const {
    return minTimeStepDuration_;
}

float EnsembleDataset::getMaxTimeStepDuration() const {
    return maxTimeStepDuration_;
}

float EnsembleDataset::getStartTime() const {
    return startTime_;
}

float EnsembleDataset::getEndTime() const {
    return endTime_;
}

float EnsembleDataset::getMaxTotalDuration() const {
    return getEndTime() - getStartTime();
}

const tgt::vec2& EnsembleDataset::getCommonTimeInterval() const {
    return commonTimeInterval_;
}

const tgt::Bounds& EnsembleDataset::getBounds() const {
    return bounds_;
}

const tgt::Bounds& EnsembleDataset::getCommonBounds() const {
    return commonBounds_;
}

const tgt::vec2& EnsembleDataset::getValueRange(const std::string& field) const {
    return getFieldMetaData(field).valueRange_;
}

const tgt::vec2& EnsembleDataset::getMagnitudeRange(const std::string& field) const {
    return getFieldMetaData(field).magnitudeRange_;
}

size_t EnsembleDataset::getNumChannels(const std::string& field) const {
    return getFieldMetaData(field).numChannels_;
}

const EnsembleFieldMetaData& EnsembleDataset::getFieldMetaData(const std::string& field) const {
    tgtAssert(fieldMetaData_.find(field) != fieldMetaData_.end(), "Field not available");
    return fieldMetaData_.at(field);
}

const std::vector<std::string>& EnsembleDataset::getUniqueFieldNames() const {
    return uniqueFieldNames_;
}

const std::vector<std::string>& EnsembleDataset::getCommonFieldNames() const {
    return commonFieldNames_;
}

std::vector<const VolumeBase*> EnsembleDataset::getVolumes() const {
    std::vector<const VolumeBase*> result;
    for(const EnsembleMember& member : members_) {
        for(const TimeStep& timeStep : member.getTimeSteps()) {
            for(const std::string& fieldName : timeStep.getFieldNames()) {
                result.push_back(timeStep.getVolume(fieldName));
            }
        }
    }
    return result;
}

std::string EnsembleDataset::toHTML() const {

    std::stringstream stream;

    stream << "<html><head>"
              "<meta content=\"text/html;charset=utf-8\" http-equiv=\"Content-Type\">\n"
              "<meta content=\"utf-8\" http-equiv=\"encoding\">\n"
              "<link src=\"https://cdn.datatables.net/1.10.20/css/jquery.dataTables.min.css\" rel=\"stylesheet\">\n"
              "<script src=\"https://code.jquery.com/jquery-3.4.1.min.js\"></script>\n"
              "<script src=\"https://cdn.datatables.net/1.10.20/js/jquery.dataTables.min.js\"></script>\n"
              "<style>table,th,td {border: 1px solid black;}</style></head>"
              "<body><table id=\"ensemble\"><thead>";
    // Parameter names
    stream << "  <tr>\n";
    // Member Name and Color are mandatory.
    stream << "    <th>Name</th>\n";
    stream << "    <th>Color</th>\n";
    stream << "    <th>Num. Time Steps</th>\n";
    stream << "    <th>Start Time</th>\n";
    stream << "    <th>End Time</th>\n";
    for(const std::string& parameter : allParameters_) {
        stream << "    <th>" << parameter << "</th>\n";

    }
    stream << "  </tr></thead><tbody>\n";

    // Members and their parameters.
    for(const EnsembleMember& member : members_) {
        stream << "  <tr>\n";
        stream << "    <th>" << member.getName() << "</th>\n";
        tgt::ivec3 color(member.getColor() * 255.0f);
        stream << "    <th style=\"background-color: rgb(" << color.r << ", " << color.g << ", " << color.b << ")\"></th>\n";
        stream << "    <th>" << member.getTimeSteps().size() << "</th>\n";
        stream << "    <th>" << member.getTimeSteps().front().getTime() << "</th>\n";
        stream << "    <th>" << member.getTimeSteps().back().getTime() << "</th>\n";

        for(const std::string& parameter : allParameters_) {
            const TimeStep& referenceTimeStep = member.getTimeSteps().front();
            // TODO: assumes that all fields contain the same parameters.
            const VolumeBase* referenceVolume = referenceTimeStep.getVolume(referenceTimeStep.getFieldNames().front());
            stream << "    <th>";
            if(referenceVolume->hasMetaData(parameter)) {
                stream << referenceVolume->getMetaData(parameter)->toString();
            }
            stream << "</th>\n";
        }
        stream << "  </tr>\n";
    }

    stream <<"</tbody></table>"
             "<script>$(document).ready( function () {$('#ensemble').DataTable({paging: false});} );</script>"
             "</body></html>";
    return stream.str();
}

void EnsembleDataset::serialize(Serializer& s) const {
    s.serialize("members", members_);
    s.serialize("uniqueFieldNames", uniqueFieldNames_);
    s.serialize("commonFieldNames", commonFieldNames_);

    s.serialize("fieldMetaData", fieldMetaData_);
    s.serialize("allParameters", allParameters_);

    s.serialize("minNumTimeSteps", minNumTimeSteps_);
    s.serialize("maxNumTimeSteps", maxNumTimeSteps_);
    s.serialize("totalNumTimeSteps", totalNumTimeSteps_);

    s.serialize("minTimeStepDuration", minTimeStepDuration_);
    s.serialize("maxTimeStepDuration", maxTimeStepDuration_);
    s.serialize("startTime", startTime_);
    s.serialize("endTime", endTime_);
    s.serialize("commonTimeInterval",commonTimeInterval_);

    s.serialize("bounds", bounds_);
    s.serialize("commonBounds", commonBounds_);
}

void EnsembleDataset::deserialize(Deserializer& s) {
    s.deserialize("members", members_);
    s.deserialize("uniqueFieldNames", uniqueFieldNames_);
    s.deserialize("commonFieldNames", commonFieldNames_);

    s.deserialize("fieldMetaData", fieldMetaData_);
    s.deserialize("allParameters", allParameters_);

    s.deserialize("minNumTimeSteps", minNumTimeSteps_);
    s.deserialize("maxNumTimeSteps", maxNumTimeSteps_);
    s.deserialize("totalNumTimeSteps", totalNumTimeSteps_);

    s.deserialize("minTimeStepDuration", minTimeStepDuration_);
    s.deserialize("maxTimeStepDuration", maxTimeStepDuration_);
    s.deserialize("startTime", startTime_);
    s.deserialize("endTime", endTime_);
    s.deserialize("commonTimeInterval",commonTimeInterval_);

    s.deserialize("bounds", bounds_);
    s.deserialize("commonBounds", commonBounds_);
}

}   // namespace
