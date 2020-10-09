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

#include "ensembleanalysisextramodule.h"

#include "processors/fieldparallelplotcreator.h"
#include "processors/fieldparallelplotviewer.h"
#include "processors/fieldparallelplothistogram.h"
#include "processors/physicalclippinglinker.h"

#include "io/fieldplotsave.h"
#include "io/fieldplotsource.h"

namespace voreen {

EnsembleAnalysisExtraModule::EnsembleAnalysisExtraModule(const std::string& modulePath)
    : VoreenModule(modulePath)
{
    setID("EnsembleAnalysisExtraExtra");
    setGuiName("EnsembleAnalysisExtraExtra");

    addShaderPath(getModulePath("glsl"));

    // Processors
    registerProcessor(new PhysicalClippingLinker());

    // Plotting
    registerProcessor(new FieldParallelPlotCreator());
    registerProcessor(new FieldParallelPlotViewer());
    registerProcessor(new FieldParallelPlotHistogram());

    // IO
    registerProcessor(new FieldPlotSave());
    registerProcessor(new FieldPlotSource());   
}

} // namespace