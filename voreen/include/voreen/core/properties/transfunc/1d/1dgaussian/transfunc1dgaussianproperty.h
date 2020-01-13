/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2020 University of Muenster, Germany,                        *
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

#ifndef VRN_TRANSFERFUNC1DGAUSSIANPROPERTY_H
#define VRN_TRANSFERFUNC1DGAUSSIANPROPERTY_H

#include "voreen/core/properties/transfunc/1d/transfunc1dproperty.h"

#include "voreen/core/datastructures/transfunc/1d/1dgaussian/transfunc1dgaussian.h"

namespace voreen {

/**
 * Property for 1d gaussian transfer functions.
 */
class VRN_CORE_API TransFunc1DGaussianProperty : public TransFunc1DProperty {
public:
    /**
     * Constructor
     *
     * @param ident identifier that is used in serialization
     * @param guiText text that is shown in the gui
     * @param invalidationLevel The owner is invalidated with this InvalidationLevel upon change.
     *
     * @note The creation of the encapsulated transfer function object is deferred
     *       to deserialization or initialization, respectively.
     */
    TransFunc1DGaussianProperty(const std::string& ident, const std::string& guiText, int invalidationLevel = Processor::INVALID_RESULT, Property::LevelOfDetail lod = Property::LOD_DEFAULT);
    TransFunc1DGaussianProperty();
    virtual ~TransFunc1DGaussianProperty();

    /**
     * Sets the stored transfer function to the given one.
     * @note The TransFuncProperty takes ownership of the passed
     *  object. Therefore, the caller must not delete it.
     * @param tf transfer function the property is set to
     */
    void set1DGaussian(TransFunc1DGaussian* tf);

    /**
     * Returns the current transfer function.
     */
    virtual TransFunc1DGaussian* get() const;

    //-------------------------------------------------
    //  Override from voreen serializable
    //-------------------------------------------------
    virtual Property* create() const {return new TransFunc1DGaussianProperty();}
    virtual std::string getClassName() const       { return "TransFunc1DGaussianProperty"; }
    virtual std::string getTypeDescription() const { return "TransferFunction"; }

    //-------------------------------------------------
    //  Override from property
    //-------------------------------------------------
    virtual void initialize();
    virtual void deinitialize();
    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

    //-------------------------------------------------
    //  Override from trans func property base
    //-------------------------------------------------
    virtual void applyTextureResizeFromData(int requestedSize);
    virtual bool isDomainFittingEnabled() const;
    virtual void applyDomainFromData();
    virtual bool isTFMetaPresent() const;
    virtual void applyTFMetaFromData();

    //-------------------------------------------------
    //  Member
    //-------------------------------------------------
protected:
    TransFunc1DGaussian* transFunc1DGaussian_;  ///< pointer to the current function.
    //-------------------------------------------------
    //  Hide base pointer of super class
    //-------------------------------------------------
private:
    using TransFunc1DProperty::transFunc1D_;
    virtual void set1D(TransFunc1D* tf) {TransFunc1DProperty::set1D(tf);}
};

} // namespace voreen

#endif // VRN_TRANSFERFUNC1DGAUSSIANPROPERTY_H
