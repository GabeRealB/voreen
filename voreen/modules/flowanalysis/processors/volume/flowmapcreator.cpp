/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2021 University of Muenster, Germany,                        *
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

#include "flowmapcreator.h"

#include "voreen/core/datastructures/volume/volumedisk.h"
#include "voreen/core/datastructures/volume/volumefactory.h"
#include "voreen/core/ports/conditions/portconditionvolumelist.h"
#include "voreen/core/utils/hashing.h"

namespace {

using namespace voreen;

class VolumeDiskMultiChannelAdapter : public VolumeDisk {
public:

    VolumeDiskMultiChannelAdapter(const std::vector<const VolumeBase*>& channels, tgt::bvec3 mirror, std::vector<size_t> swizzle, std::vector<bool> negate)
        : VolumeDisk(VolumeFactory().getFormat(channels.front()->getBaseType(), channels.size()), channels.front()->getDimensions())
        , channels_(channels)
        , mirror_(mirror)
        , swizzle_(std::move(swizzle))
        , negate_(std::move(negate))
    {
        tgtAssert(channels_.size() == swizzle_.size(), "size mismatch");
        tgtAssert(channels_.size() == negate_.size(), "size mismatch");
        const VolumeBase* ref = channels_.front();
        for (const VolumeBase* channel : channels_) {
            tgtAssert(ref->getFormat() == channel->getFormat(), "Base Type mismatch");
            tgtAssert(ref->getDimensions() == channel->getDimensions(), "Base Type mismatch");
        }
    }

    std::string getHash() const {
        std::string hash;

        for (const VolumeBase* channel : channels_) {
            hash += channel->getHash();
        }

        std::stringstream stream;
        stream << mirror_;
        stream << std::accumulate(swizzle_.begin(), swizzle_.end(), "");
        stream << std::accumulate(negate_.begin(), negate_.end(), "");

        return hash + stream.str();
    }

    VolumeRAM* loadVolume() const {
        return loadBrick(tgt::svec3::zero, dimensions_);
    }

    VolumeRAM* loadSlices(const size_t firstZSlice, const size_t lastZSlice) const {
        if (firstZSlice > lastZSlice)
            throw VoreenException("last slice must be behind first slice");

        return loadBrick(tgt::svec3(0, 0, firstZSlice),
                         tgt::svec3(dimensions_.x, dimensions_.y, lastZSlice - firstZSlice + 1));
    }

    VolumeRAM* loadBrick(const tgt::svec3& offset, const tgt::svec3& dimensions) const {
        // check parameters
        if (tgt::hmul(dimensions) == 0)
            throw VoreenException("requested brick dimensions are zero");
        if (!tgt::hand(tgt::lessThanEqual(offset + dimensions, dimensions_)))
            throw VoreenException("requested brick (at least partially) outside volume dimensions");

        // Create the output volume.
        VolumeRAM* output = VolumeFactory().create(getFormat(), dimensions);

        for (size_t channel = 0; channel < channels_.size(); channel++) {

            size_t swizzledChannel = swizzle_[channel];

            // Check if we have a ram representation already.
            if (channels_[swizzledChannel]->hasRepresentation<VolumeRAM>()) {
                VolumeRAMRepresentationLock lock(channels_[swizzledChannel]);

                tgt::svec3 pos;
                for (pos.z = 0; pos.z < dimensions.z; pos.z++) {
                    size_t z = mirror_.z ? dimensions.z - offset.z - pos.z - 1 : pos.z;
                    for (pos.y = 0; pos.y < dimensions.y; pos.y++) {
                        size_t y = mirror_.y ? dimensions.y - offset.y - pos.y - 1 : pos.y;
                        for (pos.x = 0; pos.x < dimensions.x; pos.x++) {
                            size_t x = mirror_.x ? dimensions.x - offset.x - pos.x - 1 : pos.x;
                            float value = lock->getVoxelNormalized(x, y, z);
                            if (negate_[swizzledChannel]) {
                                value = -value;
                            }
                            output->setVoxelNormalized(value, pos, channel);
                        }
                    }
                }
            } else if (const VolumeDisk* vd = channels_[swizzledChannel]->getRepresentation<VolumeDisk>()) {
                tgt::svec3 effOffset = offset;
                effOffset.x = mirror_.x ? dimensions_.x - dimensions.x - offset.x : offset.x;
                effOffset.y = mirror_.y ? dimensions_.y - dimensions.y - offset.y : offset.y;
                effOffset.z = mirror_.z ? dimensions_.z - dimensions.z - offset.z : offset.z;

                std::unique_ptr<VolumeRAM> brick(vd->loadBrick(effOffset, dimensions));
                tgt::svec3 pos;
                for (pos.z = 0; pos.z < dimensions.z; pos.z++) {
                    size_t z = mirror_.z ? dimensions.z - pos.z - 1 : pos.z;
                    for (pos.y = 0; pos.y < dimensions.y; pos.y++) {
                        size_t y = mirror_.y ? dimensions.y - pos.y - 1 : pos.y;
                        for (pos.x = 0; pos.x < dimensions.x; pos.x++) {
                            size_t x = mirror_.x ? dimensions.x - pos.x - 1 : pos.x;
                            float value = brick->getVoxelNormalized(x, y, z);
                            if (negate_[swizzledChannel]) {
                                value = -value;
                            }
                            output->setVoxelNormalized(value, pos, channel);
                        }
                    }
                }
            } else {
                tgtAssert(false, "Could not get representation for channel");
            }
        }

        return output;
    }

private:

    std::vector<const VolumeBase*> channels_;
    tgt::bvec3 mirror_;
    std::vector<size_t> swizzle_;
    std::vector<bool> negate_;
};

}

namespace voreen {

FlowMapCreator::FlowMapCreator()
    : Processor()
    , inport_(Port::INPORT, "volumelist.input", "Volume List Input", false)
    , outport_(Port::OUTPORT, "volumelist.output", "Volume List Output ", false)
    , numChannels_("numChannels", "Num. Channels", 3, 1, 4)
    , layout_("layout", "Layout", Processor::INVALID_RESULT, false, Property::LOD_ADVANCED)
    , mirrorX_("mirrorX", "Mirror X", false)
    , mirrorY_("mirrorY", "Mirror Y", false)
    , mirrorZ_("mirrorZ", "Mirror Z", false)
    , swizzleChannel1_("swizzleChannel1", "Swizzle Channel 1")
    , swizzleChannel2_("swizzleChannel2", "Swizzle Channel 2")
    , swizzleChannel3_("swizzleChannel3", "Swizzle Channel 3")
    , swizzleChannel4_("swizzleChannel4", "Swizzle Channel 4")
    , negateChannel1_("negateChannel1", "Negate Channel 1", false)
    , negateChannel2_("negateChannel2", "Negate Channel 2", false)
    , negateChannel3_("negateChannel3", "Negate Channel 3", false)
    , negateChannel4_("negateChannel4", "Negate Channel 4", false)
{
    addPort(inport_);
    inport_.addCondition(new PortConditionVolumeListEnsemble());
    inport_.addCondition(new PortConditionVolumeListAdapter(new PortConditionVolumeChannelCount(1)));
    addPort(outport_);

    addProperty(numChannels_);
    ON_CHANGE(numChannels_, FlowMapCreator, onChannelCountChanged);
    addProperty(layout_);
    layout_.addOption("xyzxyz", "xyzxyz");
    layout_.addOption("xxyyzz", "xxyyzz");

    addProperty(mirrorX_);
    addProperty(mirrorY_);
    addProperty(mirrorZ_);

    addProperty(swizzleChannel1_);
    addProperty(swizzleChannel2_);
    addProperty(swizzleChannel3_);
    addProperty(swizzleChannel4_);

    addProperty(negateChannel1_);
    addProperty(negateChannel2_);
    addProperty(negateChannel3_);
    addProperty(negateChannel4_);

    // Update GUI according to initial state.
    onChannelCountChanged();
}

FlowMapCreator::~FlowMapCreator() {}

Processor* FlowMapCreator::create() const {
    return new FlowMapCreator();
}

void FlowMapCreator::onChannelCountChanged() {
    //swizzleChannel1_.setVisibleFlag(numChannels_.get() > 0); // always true.
    swizzleChannel2_.setVisibleFlag(numChannels_.get() > 1);
    swizzleChannel3_.setVisibleFlag(numChannels_.get() > 2);
    swizzleChannel4_.setVisibleFlag(numChannels_.get() > 3);

    //negateChannel1_.setVisibleFlag(numChannels_.get() > 0); // always true.
    negateChannel2_.setVisibleFlag(numChannels_.get() > 1);
    negateChannel3_.setVisibleFlag(numChannels_.get() > 2);
    negateChannel4_.setVisibleFlag(numChannels_.get() > 3);

    std::deque<Option<size_t>> options;
    options.push_back(Option<size_t>("x", "x", 0));
    if(numChannels_.get() > 1) {
        options.push_back(Option<size_t>("y", "y", 1));
    }
    if(numChannels_.get() > 2) {
        options.push_back(Option<size_t>("z", "z", 2));
    }
    if(numChannels_.get() > 3) {
        options.push_back(Option<size_t>("w", "w", 3));
    }

    OptionProperty<size_t>* swizzleProperties[] = {&swizzleChannel1_, &swizzleChannel2_, &swizzleChannel3_, &swizzleChannel4_};
    for(size_t propId = 0; propId < static_cast<size_t>(numChannels_.get()); propId++) {
        bool wasSetBefore = !swizzleProperties[propId]->getOptions().empty();
        swizzleProperties[propId]->setOptions(options);
        if(!wasSetBefore) {
            swizzleProperties[propId]->selectByValue(propId);
        }
    }
}

void FlowMapCreator::process() {

    const VolumeList* input = inport_.getData();
    tgtAssert(input, "no input");

    // Clear old data (order matters!).
    outport_.clear();
    volumes_.clear();

    size_t numChannels = static_cast<size_t>(numChannels_.get());
    size_t numVolumes = input->size() / numChannels; // floor(x).

    tgt::bvec3 mirror;
    mirror.x = mirrorX_.get();
    mirror.y = mirrorY_.get();
    mirror.z = mirrorZ_.get();

    std::vector<size_t> swizzle;
    std::vector<bool> negate;
    swizzle.push_back(swizzleChannel1_.getValue());
    negate.push_back(negateChannel1_.get());
    if(numChannels > 1) {
        swizzle.push_back(swizzleChannel2_.getValue());
        negate.push_back(negateChannel2_.get());
    }
    if(numChannels > 2) {
        swizzle.push_back(swizzleChannel3_.getValue());
        negate.push_back(negateChannel3_.get());
    }
    if(numChannels > 3) {
        swizzle.push_back(swizzleChannel4_.getValue());
        negate.push_back(negateChannel4_.get());
    }

    VolumeList* output = new VolumeList();

    for (size_t i = 0; i < numVolumes; i++) {

        std::vector<const VolumeBase*> channels;
        if (layout_.get() == "xyzxyz") {
            for (size_t channel = 0; channel < numChannels; channel++) {
                size_t index = i * numChannels + channel;
                channels.push_back(input->at(index));
            }
        }
        else if (layout_.get() == "xxyyzz") {
            for (size_t channel = 0; channel < numChannels; channel++) {
                size_t index = channel * i + numVolumes;
                channels.push_back(input->at(index));
            }
        }
        else {
            tgtAssert(false, "unknown layout");
        }

        VolumeDisk* vd = new VolumeDiskMultiChannelAdapter(channels, mirror, swizzle, negate);
        VolumeBase* volume = new Volume(vd, input->first());
        output->add(volume);

        // Transfer ownership.
        volumes_.push_back(std::unique_ptr<VolumeBase>(volume));
    }

    outport_.setData(output, true);
}

}   // namespace