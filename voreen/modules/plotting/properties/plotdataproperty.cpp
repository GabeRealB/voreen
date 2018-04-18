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

#include "plotdataproperty.h"

#include "../datastructures/plotdata.h"

namespace voreen {

PlotDataProperty::PlotDataProperty(const std::string& id, const std::string& guiText,
                                   const PlotData* value,
                                   Processor::InvalidationLevel invalidationLevel) :
    TemplateProperty<const PlotData*>(id, guiText, value, invalidationLevel)
{
}

PlotDataProperty::PlotDataProperty()
    : TemplateProperty<const PlotData*>("", "", 0, Processor::INVALID_RESULT)
{}

Property* PlotDataProperty::create() const {
    return new PlotDataProperty();
}

void PlotDataProperty::set(const voreen::PlotData* const& data) {
    value_ = data;
    notifyChange();
}

void PlotDataProperty::notifyChange() {
    updateWidgets();
    invalidateOwner();
}

} // namespace voreen
