/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2014 University of Muenster, Germany.                        *
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

#ifndef VRN_COSMOLOGYPARTICLECOMPACTOR_H
#define VRN_COSMOLOGYPARTICLECOMPACTOR_H

#include "../ports/cmparticleport.h"

#include "voreen/core/processors/processor.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/boundingboxproperty.h"



namespace voreen {

class VRN_CORE_API CosmologyParticleCompactor : public Processor {

public:
    CosmologyParticleCompactor();

    virtual Processor*  create() const       { return new CosmologyParticleCompactor(); }
    virtual std::string getClassName() const { return "CosmologyParticleCompactor";     }
    virtual std::string getCategory() const  { return "Viscontest2019";                 }

protected:
    virtual void setDescriptions() { setDescription("Processor that compacts particle indices."); }
    virtual void process();
private:
    //-------------
    //  members
    //-------------
    CMParticlePort outport_;
    CMParticlePort inport_;

};

} // namespace

#endif // VRN_COSMOLOGYPARTICLEBOXFILTER_H