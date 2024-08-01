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

#ifndef VRN_VOLUMEDISTANCETRANSFORM_H
#define VRN_VOLUMEDISTANCETRANSFORM_H

#include <string>
#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/properties/boolproperty.h"


namespace voreen {

class Volume;

class VRN_CORE_API VolumeDistanceTransform : public CachingVolumeProcessor {
public:
    VolumeDistanceTransform();
    virtual Processor* create() const;

    virtual std::string getClassName() const  { return "VolumeDistanceTransform"; }
    virtual std::string getCategory() const   { return "Volume Processing";       }
    virtual CodeState getCodeState() const    { return CODE_STATE_TESTING;        }

protected:
    virtual void setDescriptions() {
        setDescription("Performs a 3D distance transform of the input volume.");
    }

    virtual void process();

private:
    void distanceTransform();

private:
    VolumePort inport_;
    VolumePort outport_;

    BoolProperty enableProcessing_;
};

}   //namespace

#endif // VRN_VOLUMEDISTANCETRANSFORM_H
