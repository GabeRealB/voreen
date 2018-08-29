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

#ifndef VRN_VESSELGRAPHNORMALIZER_H
#define VRN_VESSELGRAPHNORMALIZER_H

#include "voreen/core/processors/processor.h"

#include "../ports/vesselgraphport.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/optionproperty.h"

namespace voreen {

class VesselGraphNormalizer : public Processor {
public:
    VesselGraphNormalizer();
    virtual ~VesselGraphNormalizer();
    virtual std::string getCategory() const { return "Geometry"; }
    virtual std::string getClassName() const { return "VesselGraphNormalizer"; }
    virtual CodeState getCodeState() const { return Processor::CODE_STATE_EXPERIMENTAL; }
    virtual Processor* create() const { return new VesselGraphNormalizer(); }

protected:
    virtual void setDescriptions() {
        setDescription("This processor can be used to remove small, unwanted edges from Vessel-Graphs.");
    }

    enum NormalizationMethod {
        ALL,
        END_RECURSIVE
    };

    virtual void process();

    std::function<bool(const VesselGraphEdge& edge)> createRemovableEdgePredicate() const;

    //void adjustPropertiesToInput();

    VesselGraphPort inport_;
    VesselGraphPort outport_;

    // properties
    BoolProperty enabled_;
    OptionProperty<NormalizationMethod> normalizationMethod_;
    IntProperty minVoxelLength_;
    FloatProperty minElongation_;
    FloatProperty minBulgeSize_;

    static const std::string loggerCat_;
};

} // namespace voreen
#endif // VRN_VESSELGRAPHNORMALIZER_H
