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

#ifndef VRN_THRESHOLDINGFILTERPROPERTIES_H
#define VRN_THRESHOLDINGFILTERPROPERTIES_H

#include "filterproperties.h"

#include "../volumefiltering/thresholdingfilter.h"

namespace voreen {

class ThresholdingFilterProperties : public FilterProperties {
public:
    ThresholdingFilterProperties();

    virtual std::string getVolumeFilterName() const;

    virtual void adjustPropertiesToInput(const VolumeBase& input);

    virtual VolumeFilter* getVolumeFilter(const VolumeBase& volume, int instanceId) const;
    virtual void restoreInstance(int instanceId);
    virtual void storeInstance(int instanceId);
    virtual void removeInstance(int instanceId);
    virtual void addProperties();
    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

private:

    struct Settings {
        float thresholdValue_;
        bool binarize_;
        float replacementValue_;
        ThresholdingStrategyType thresholdingStrategyType_;
    };
    std::map<int, Settings> instanceSettings_;

    FloatProperty thresholdValue_;
    BoolProperty binarize_;
    FloatProperty replacementValue_;
    OptionProperty<ThresholdingStrategyType> thresholdingStrategyType_;
};

}

#endif