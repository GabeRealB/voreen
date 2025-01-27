/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
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

#include "vortextracking.h"

#include <chrono>

#include "voreen/core/datastructures/geometry/pointlistgeometry.h"
#include "voreen/core/datastructures/geometry/pointsegmentlistgeometry.h"

namespace voreen {

VortexTracking::VortexTracking() : Processor(),
                                   _inVortex(Port::INPORT, "inVortex", "Vortex for which the corresponding vortex should be found in the given list of vortices"),
                                   _inVortices(Port::INPORT, "inVortices", "The vortices to compare"),
                                   _outTrackedCoreline(Port::OUTPORT, "outTrackedCoreline", "If found, a coreline that corresponds to the input coreline at the next timestep"),
                                   _maxDistanceSameCoreline("maxDistanceOfSameCorelineAtTwoConsecutiveTimesteps", "Maximum distance of the same coreline at two consecutive timesteps", 5.0f, 0.0f, 10.0f)
{
    this->addPort(_inVortex);
    this->addPort(_inVortices);
    this->addPort(_outTrackedCoreline);
    this->addProperty(_maxDistanceSameCoreline);
}

void VortexTracking::Process(const Vortex &vortex, const std::vector<Vortex> &vortices, float maxDistanceSameCoreline, size_t &outTrackedVortexIndex)
{
    auto minDistanceBetweenCorelines = std::numeric_limits<float>::max();
    outTrackedVortexIndex = std::numeric_limits<size_t>::max();

    for (size_t compareVortexIndex = 0; compareVortexIndex < vortices.size(); ++compareVortexIndex)
    {
        const auto &compareVortex = vortices[compareVortexIndex];
        if (compareVortex.getOrientation() != vortex.getOrientation())
            continue;

        auto sumOfMinDistancesBetweenPoints = 0.0f;
        for (const auto point : vortex.coreline())
        {
            auto minDistanceBetweenPoints = std::numeric_limits<float>::max();
            for (const auto compareCorelinePoint : compareVortex.coreline())
            {
                const auto distance = tgt::length(point - compareCorelinePoint);
                if (minDistanceBetweenPoints > distance)
                    minDistanceBetweenPoints = distance;
            }
            sumOfMinDistancesBetweenPoints += minDistanceBetweenPoints;
        }
        const auto avgDistanceBetweenPoints = sumOfMinDistancesBetweenPoints / vortex.coreline().size();

        if (minDistanceBetweenCorelines > avgDistanceBetweenPoints && avgDistanceBetweenPoints <= maxDistanceSameCoreline)
        {
            minDistanceBetweenCorelines = avgDistanceBetweenPoints;
            outTrackedVortexIndex = compareVortexIndex;
        }
    }
}

void VortexTracking::process()
{
    if (!_inVortex.hasData() || !_inVortices.hasData() || !_inVortex.getData()->coreline().size() || !_inVortices.getData()->size())
        return;

    size_t index;

    VortexTracking::Process(*_inVortex.getData(), *_inVortices.getData(), _maxDistanceSameCoreline.get(), index);

    auto out = std::unique_ptr<PointListGeometryVec3>(new PointListGeometryVec3());
    if (index != std::numeric_limits<size_t>::max())
        out->setData((*_inVortices.getData())[index].coreline());

    _outTrackedCoreline.setData(out.release());
}

} // namespace voreen
