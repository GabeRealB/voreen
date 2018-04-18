/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
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

#ifndef VRN_SEEDPOINTGENERATOR_H
#define VRN_SEEDPOINTGENERATOR_H

#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/ports/volumeport.h"
#include "voreen/core/ports/geometryport.h"
#include "voreen/core/datastructures/geometry/pointlistgeometry.h"

#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/vectorproperty.h"

#include "voreen/core/ports/volumeport.h"

namespace voreen {

class SeedpointGenerator : public VolumeProcessor {
public:
    SeedpointGenerator();
    virtual Processor* create() const;

    virtual std::string getClassName() const { return "SeedpointGenerator";    }
    virtual std::string getCategory() const  { return "Utility";          }
    virtual CodeState getCodeState() const   { return CODE_STATE_EXPERIMENTAL; }
    virtual bool isUtility() const           { return true; }

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& d);

protected:
    virtual void setDescriptions() {
        setDescription("");
    }

    void process();

private:
    void addSeed();
    void clearLast();
    void clearAll();
    void updateProperties();

    VolumePort inportVolume_;
    GeometryPort outportSeeds_;

    ButtonProperty addSeed_;
    ButtonProperty clearLast_;
    ButtonProperty clearAll_;
    IntProperty numSeeds_;
    FloatVec3Property seedPoint_;

    std::vector<tgt::vec3> seedPoints_;

    static const std::string loggerCat_;
};

} // namespace voreen

#endif //VRN_SEEDPOINTGENERATOR_H
