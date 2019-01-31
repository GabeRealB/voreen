/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2017 University of Muenster, Germany.                        *
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


#include "ensemblehash.h"

namespace voreen {

EnsembleHash::EnsembleHash(const EnsembleDataset& dataset)
    : hash_("")
{
    std::string unhashed;
    for (const EnsembleDataset::Run& run : dataset.getRuns()) {
        unhashed += run.name_;
        unhashed += std::to_string(run.timeSteps_.size());
    }

    for (const std::string& channel : dataset.getCommonChannels())
        unhashed += channel;

    hash_ = VoreenHash::getHash(unhashed);
}

const std::string& EnsembleHash::getHash() const {
    return hash_;
}

}