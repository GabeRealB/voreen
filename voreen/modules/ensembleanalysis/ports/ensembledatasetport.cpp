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

#include "ensembledatasetport.h"

namespace voreen {

EnsembleDatasetPort::EnsembleDatasetPort(PortDirection direction, const std::string& id, const std::string& guiName, bool allowMultipleConnections, Processor::InvalidationLevel invalidationLevel)
    : GenericPort<EnsembleDataset>(direction, id, guiName, allowMultipleConnections, invalidationLevel) {
}

std::string EnsembleDatasetPort::getClassName() const {
    return "EnsembleDatasetPort";
}

Port* EnsembleDatasetPort::create(PortDirection direction, const std::string& id, const std::string& guiName) const {
    return new EnsembleDatasetPort(direction,id,guiName);
}

tgt::col3 EnsembleDatasetPort::getColorHint() const {
    return tgt::col3(24, 72, 124);
}

std::string EnsembleDatasetPort::getContentDescription() const {
    std::stringstream strstr;
    strstr << Port::getContentDescription();
    if(hasData()) {
        if (!getData()->getRuns().empty()) {
            strstr << std::endl << "Number of runs: " << getData()->getRuns().size();
            strstr << std::endl << "Number of unique Fields: " << getData()->getUniqueFieldNames().size();
            strstr << std::endl << "Number of common Fields: " << getData()->getCommonFieldNames().size();
            strstr << std::endl << "Min Number of Time Steps: " << getData()->getMinNumTimeSteps();
            strstr << std::endl << "Max Number of Time Steps: " << getData()->getMaxNumTimeSteps();
            strstr << std::endl << "Start Time: " << getData()->getStartTime();
            strstr << std::endl << "End Time: " << getData()->getEndTime();
        }
        else
            strstr << std::endl << "Empty Ensemble Dataset";
    }
    return strstr.str();
}

std::string EnsembleDatasetPort::getContentDescriptionHTML() const {
    std::stringstream strstr;
    strstr << Port::getContentDescriptionHTML();
    if(hasData()) {
        if (!getData()->getRuns().empty()) {
            strstr << "<br>" << "Number of runs: " << getData()->getRuns().size();
            strstr << "<br>" << "Number of unique Fields: " << getData()->getUniqueFieldNames().size();
            strstr << "<br>" << "Number of common Fields: " << getData()->getCommonFieldNames().size();
            strstr << "<br>" << "Min Number of Time Steps: " << getData()->getMinNumTimeSteps();
            strstr << "<br>" << "Max Number of Time Steps: " << getData()->getMaxNumTimeSteps();
            strstr << "<br>" << "Start Time: " << getData()->getStartTime();
            strstr << "<br>" << "End Time: " << getData()->getEndTime();
        }
        else
            strstr << "<br>" << "Empty Ensemble Dataset";
    }
    return strstr.str();
}

} // namespace