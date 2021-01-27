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

#ifndef VRN_VOLUMEOPERATORUNCERTAINTYMEASURE_H
#define VRN_VOLUMEOPERATORUNCERTAINTYMEASURE_H

#include "voreen/core/datastructures/volume/volumeoperator.h"

namespace voreen {

// Base class, defines interface for the operator (-> apply):
class VRN_CORE_API VolumeOperatorUncertaintyMeasureBase : public UnaryVolumeOperatorBase {
public:
    virtual Volume* apply(const VolumeBase* volume, ProgressReporter* progressReporter = 0) const = 0;
};

// Generic implementation:
template<typename T>
class VolumeOperatorUncertaintyMeasureGeneric : public VolumeOperatorUncertaintyMeasureBase {
public:
    virtual Volume* apply(const VolumeBase* volume, ProgressReporter* progressReporter = 0) const;
    //Implement isCompatible using a handy macro:
    IS_COMPATIBLE
};

template<typename T>
Volume* VolumeOperatorUncertaintyMeasureGeneric<T>::apply(const VolumeBase* vh, ProgressReporter* progressReporter) const {
    const VolumeAtomic<T>* va = dynamic_cast<const VolumeAtomic<T>*>(vh->getRepresentation<VolumeRAM>());
    if (!va)
        return nullptr;

    VolumeAtomic<T>* out = va->clone();

    VRN_FOR_EACH_VOXEL_WITH_PROGRESS(index, tgt::svec3::zero, out->getDimensions(), progressReporter) {
        out->voxel(index) = T(1) - tgt::abs(T(2)*out->voxel(index) - T(1));
    }
    if (progressReporter)
        progressReporter->setProgress(1.f);

    return new Volume(out, vh);
}

typedef UniversalUnaryVolumeOperatorGeneric<VolumeOperatorUncertaintyMeasureBase> VolumeOperatorUncertaintyMeasure;

} // namespace

#endif // VRN_VOLUMEOPERATORUNCERTAINTYMEASURE_H
