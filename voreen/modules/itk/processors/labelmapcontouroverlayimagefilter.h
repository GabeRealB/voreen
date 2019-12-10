/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2019 University of Muenster, Germany,                        *
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

#ifndef VRN_LABELMAPCONTOUROVERLAYIMAGEFILTER_H
#define VRN_LABELMAPCONTOUROVERLAYIMAGEFILTER_H

#include "modules/itk/processors/itkprocessor.h"
#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/ports/volumeport.h"
#include "voreen/core/ports/geometryport.h"
#include <string>

#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/intproperty.h"


namespace voreen {

class VolumeBase;

class LabelMapContourOverlayImageFilterITK : public ITKProcessor {
public:
    LabelMapContourOverlayImageFilterITK();

    virtual Processor* create() const;

    virtual std::string getCategory() const   { return "Volume Processing/Filtering/ImageFusion"; }
    virtual std::string getClassName() const  { return "LabelMapContourOverlayImageFilterITK";  }
    virtual CodeState getCodeState() const    { return CODE_STATE_STABLE; }

protected:
    virtual void setDescriptions() {
        setDescription("");
    }
    template<class T, class S>
    void labelMapContourOverlayImageFilterITK();


    virtual void process();
    template<class T>
    void volumeTypeSwitch1();


private:
    VolumePort inport1_;
    VolumePort inport2_;
    VolumePort outport1_;

    IntProperty numObjects_;
    FloatProperty opacity_;


    static const std::string loggerCat_;
};
}
#endif
