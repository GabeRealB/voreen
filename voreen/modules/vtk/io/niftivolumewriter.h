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

#ifndef VRN_NIFTIVOLUMEWRITER_H
#define VRN_NIFTIVOLUMEWRITER_H

#include "vtivolumewriter.h"

namespace voreen {

/**
* This reader is capable of writing NifTI-1 files.
*/
class VRN_CORE_API NiftiVolumeWriter : public VolumeWriter {
public:
    NiftiVolumeWriter(ProgressBar* progress = 0);

    virtual VolumeWriter* create(ProgressBar* progress = 0) const;

    virtual std::string getClassName() const { return "NiftiVolumeWriter"; }
    virtual std::string getFormatDescription() const { return "NifTI-1 Format format"; }

    virtual void write(const std::string& fileName, const VolumeBase* volumeHandle);

private:

    static const std::string loggerCat_;
};

} // namespace voreen

#endif // VRN_NIFTIVOLUMEREADER_H
