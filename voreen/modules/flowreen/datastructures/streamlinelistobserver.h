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

#ifndef VRN_STREAMLINELISTOBSERVER_H
#define VRN_STREAMLINELISTOBSERVER_H

#include "voreen/core/utils/observer.h"

namespace voreen {

    class StreamlineListBase;
    class StreamlineListDecoratorIdentity;

/**
 * Interface for streamlinelist observers.
 */
class VRN_CORE_API StreamlineListObserver : public Observer {
public:
    /**
     * This method is called by the observed StreamlineListBase's destructor.
     *
     * @param source the calling StreamlineListBase
     */
    virtual void beforeStreamlineListDelete(const StreamlineListBase* source) = 0;

    /**
     * This method is called by the observed StreamlineListDeorator's on beforeStreamlineListDelete.
     *
     * @param source the calling StreamlineListDecorator
     */
    virtual void beforeStreamlineListDecoratorPointerReset(const StreamlineListDecoratorIdentity* source) = 0;
};

} //namespace

#endif
