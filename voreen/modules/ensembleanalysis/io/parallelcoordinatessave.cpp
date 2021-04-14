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

#include "parallelcoordinatessave.h"

namespace voreen {

ParallelCoordinatesSave::ParallelCoordinatesSave()
        : Processor()
        , _inport( Port::INPORT, "inport", "Parallel Coordinates Axes" )
        , _propertyFileDialog( "property_file_dialog", "File Output", "Select File...", "", "Voreen Parallel Coordinates (*.vpc)", FileDialogProperty::SAVE_FILE )
        , _propertySaveButton( "property_save_button", "Save" )
{
    this->addPort( _inport );

    this->addProperty( _propertyFileDialog );
    this->addProperty( _propertySaveButton );
}

Processor* ParallelCoordinatesSave::create() const {
    return new ParallelCoordinatesSave();
}

void ParallelCoordinatesSave::process() {
    if( !_propertyFileDialog.get().empty() && _inport.hasData())
        _inport.getData()->serialize(_propertyFileDialog.get());
}

}