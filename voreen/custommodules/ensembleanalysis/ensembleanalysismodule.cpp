/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2015 University of Muenster, Germany.                        *
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

#include "ensembleanalysismodule.h"

#include "processors/ensembledatasource.h"
#include "processors/ensemblefilter.h"
#include "processors/ensemblevolumeextractor.h"
#include "processors/ensemblesimilarityplot.h"
#include "processors/fieldparallelplotcreator.h"
#include "processors/fieldparallelplotviewer.h"
#include "processors/fieldparallelplothistogram.h"
#include "processors/mdsplot.h"
#include "processors/volumeintensityfilter.h"
#include "processors/probabilityvolumecreator.h"
#include "processors/similaritydatavolume.h"
#include "processors/volumelistmerger.h"
#include "processors/volumemerger.h"
#include "processors/waveheightextractor.h"

#include "io/fieldplotsave.h"
#include "io/fieldplotsource.h"
#ifdef VRN_USE_VTK
#include "io/vtivolumereader.h"
#include "io/vtmvolumereader.h"
#endif

#include "custommodules/ensembleanalysis/properties/link/ensembleanalysislinkevaluatorid.h"

namespace voreen {

EnsembleAnalysisModule::EnsembleAnalysisModule(const std::string& modulePath)
    : VoreenModule(modulePath)
{
    setID("EnsembleAnalysis");
    setGuiName("EnsembleAnalysis");

    addShaderPath(getModulePath("glsl"));

    // Processors
    registerProcessor(new EnsembleDataSource());
    registerProcessor(new EnsembleFilter);
    registerProcessor(new SimilartyDataVolume());

    // Plotting
    registerProcessor(new FieldParallelPlotCreator());
    registerProcessor(new EnsembleSimilarityPlot());
    registerProcessor(new FieldParallelPlotViewer());
    registerProcessor(new FieldParallelPlotHistogram());
    registerProcessor(new MDSPlot());
    registerProcessor(new VolumeIntensityFilter());
    registerProcessor(new ProbabilityVolumeCreator());

    // IO
    registerProcessor(new FieldPlotSave());
    registerProcessor(new FieldPlotSource());
#ifdef VRN_USE_VTK
    registerVolumeReader(new VTIVolumeReader());
    registerVolumeReader(new VTMVolumeReader());
#endif
    
    // Properties
    registerSerializableType(new LinkEvaluatorIntListId());

    // Misc
    registerProcessor(new WaveHeightExtractor());
    registerProcessor(new EnsembleVolumeExtractor());
    registerProcessor(new VolumeListMerger());
    registerProcessor(new VolumeMerger());
}

} // namespace
