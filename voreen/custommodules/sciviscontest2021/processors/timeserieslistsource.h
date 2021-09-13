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

#ifndef VRN_TIMESERIESLISTSOURCE_H
#define VRN_TIMESERIESLISTSOURCE_H

#include "voreen/core/processors/processor.h"

#include "custommodules/sciviscontest2021/ports/timeserieslistport.h"

namespace voreen {
class VRN_CORE_API TimeSeriesListSource : public Processor {
public:
    TimeSeriesListSource();

    virtual Processor* create() const         { return new TimeSeriesListSource(); }
    virtual std::string getClassName() const  { return "TimeSeriesListSource"; }
    virtual std::string getCategory() const   { return "Input"; }
    virtual CodeState getCodeState() const    { return CODE_STATE_EXPERIMENTAL; }

protected:

    virtual void process();
    virtual void invalidate(int inv = 1);

private:

    void loadTimeSeriesList();
    TimeSeriesListPort outport_;

    FileDialogProperty filenameProp_;               ///< determines the name of the saved file
    ButtonProperty loadButton_;                     ///< triggers a load

    bool loadTimeSeriesList_;          ///< used to determine, if process should load or not

    static const std::string loggerCat_;
};
} // namespace voreen


#endif //VRN_TIMESERIESLISTSOURCE_H