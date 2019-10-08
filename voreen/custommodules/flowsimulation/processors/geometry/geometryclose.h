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

#ifndef VRN_GEOMETRYCLOSE_H
#define VRN_GEOMETRYCLOSE_H

#include "voreen/core/processors/processor.h"

#include "voreen/core/ports/geometryport.h"

#include "voreen/core/properties/boolproperty.h"

namespace voreen {

class GlMeshGeometryUInt32Normal;

/**
 * Closes all holes of the input geometry.
 */
class VRN_CORE_API GeometryClose : public Processor {
public:
    GeometryClose();
    virtual ~GeometryClose();
    virtual Processor* create() const;

    virtual std::string getClassName() const { return "GeometryClose";          }
    virtual std::string getCategory() const  { return "Geometry";               }
    virtual CodeState getCodeState() const   { return CODE_STATE_EXPERIMENTAL;  }

protected:
    virtual void setDescriptions() {
        setDescription("Closes all holes of the input geometry.");
    }

    virtual void process();

    /**
     * Creates indices for this mesh, if not already available.
     * This will enable indexed drawing.
     * The vertices stay untouched, such that non-indexed drawing yields the same result.
     *
     * The indices can be optimized such that duplicated vertices will have
     * the very same index pointing to them.
     *
     * @param geometry Geometry for that indices shall be created
     * @param optimize determines, whether indices should be optimized
     */
    void createIndices(GlMeshGeometryUInt32Normal* geometry, bool optimize) const;

    GeometryPort inport_;        ///< Inport for a list of mesh geometries to close.
    GeometryPort outport_;       ///< Outport for a list of mesh geometries that were closed.

    BoolProperty enabled_;       ///< Determines whether the closing operation is performed.
    FloatProperty epsilon_;      ///< Epsilon for equality check.

    /// category used in logging
    static const std::string loggerCat_;
};

} //namespace

#endif // VRN_GEOMETRYCLOSE_H
