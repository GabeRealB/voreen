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

#ifndef VRN_PWIVOLUME_H
#define VRN_PWIVOLUME_H

#include "voreen/core/processors/volumeprocessor.h"
#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/optionproperty.h"

#include "modules/plotting/datastructures/plotdata.h"
#include "modules/plotting/datastructures/plotcell.h"
#include "modules/plotting/ports/plotport.h"

namespace voreen {

class PWIVolume : public CachingVolumeProcessor {

public:

    PWIVolume();
    ~PWIVolume();
    virtual Processor* create() const;

    virtual std::string getClassName() const      { return "PWIVolume";     }
    virtual std::string getCategory() const       { return "Volume Processing"; }
    virtual CodeState getCodeState() const        { return CODE_STATE_EXPERIMENTAL;   }
    virtual bool usesExpensiveComputation() const { return true; }

protected:

    virtual void setDescriptions() {
        setDescription("Combines two volumes based on a selectable function.");
    }

    virtual void process();

private:

    /**
     * Computes the perfusion integral over a time series of volumes.
     */
    void computePerfusion(const VolumeList* combinedVolume);

    VolumeListPort inport_;
    VolumePort outport_;
    PlotPort plotOutport_;

    BoolProperty enableProcessing_;

    static const std::string loggerCat_; ///< category used in logging
};


} // namespace

#endif // VRN_PWIVOLUME_H
