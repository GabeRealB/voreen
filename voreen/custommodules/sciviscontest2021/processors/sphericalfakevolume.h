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

#ifndef VRN_SPHERICALFAKEVOLUME_H
#define VRN_SPHERICALFAKEVOLUME_H

#include <voreen/core/datastructures/volume/volumeatomic.h>
#include <modules/base/processors/volume/volumecreatebase.h>
#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/vectorproperty.h"

namespace voreen {

class Volume;

class VRN_CORE_API SphericalFakeVolume : public VolumeCreateBase {
public:
    SphericalFakeVolume();
    virtual Processor* create() const;

    virtual std::string getCategory() const   { return "Volume Processing"; }
    virtual std::string getClassName() const  { return "SphericalFakeVolume";      }
    virtual CodeState getCodeState() const    { return CODE_STATE_STABLE;   }

protected:
    virtual void setDescriptions() {
        setDescription("Creates a Volume and modifies its spacing and offset depending on the radius and shift factor.");
    }

    virtual void process();

private:

    VolumePort outport_;

    FloatProperty radiusMax_;
    FloatProperty shiftFactor_;

    ButtonProperty regenerate_;

    static const std::string loggerCat_; ///< category used in logging
};

}   //namespace

#endif // VRN_SPHERICALFAKEVOLUME_H