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

#ifndef VRN_VOLUMETIMESTEP_H
#define VRN_VOLUMETIMESTEP_H

#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/properties/boolproperty.h"

namespace voreen {

class VRN_CORE_API VolumeTimestep : public VolumeProcessor, public PortObserver {
public:
    VolumeTimestep();
    virtual ~VolumeTimestep();
    virtual Processor* create() const;

    virtual std::string getCategory() const   { return "Volume Processing";     }
    virtual std::string getClassName() const  { return "VolumeTimestep";        }
    virtual CodeState getCodeState() const    { return CODE_STATE_EXPERIMENTAL; }

protected:
    virtual void setDescriptions() {
        setDescription("Modifies the volume's timestep.");
    }

    virtual void process();

    virtual void adjustPropertiesToInput();

    virtual void afterConnectionAdded(const Port* source, const Port* connectedPort) {}
    virtual void beforeConnectionRemoved(const Port* source, const Port*) {}
    virtual void dataWillChange(const Port* source);
    //virtual void dataHasChanged(const Port* source) {};

private:
    void resetTimestep();

    VolumePort inport_;
    VolumePort outport_;

    BoolProperty enableProcessing_;
    FloatProperty timestep_;
    ButtonProperty reset_;

    static const std::string loggerCat_; ///< category used in logging
};

}   //namespace

#endif // VRN_VOLUMEOFFSET_H