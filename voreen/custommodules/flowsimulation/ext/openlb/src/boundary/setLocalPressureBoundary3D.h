/*  This file is part of the OpenLB library
 *
 *  Copyright (C) 2020 Alexander Schulz
 *  E-mail contact: info@openlb.net
 *  The most recent release of OpenLB can be downloaded at
 *  <http://www.openlb.net/>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
*/

///This file contains the Local Pressure Boundary
///This is a new version of the Boundary, which only contains free floating functions
#ifndef SET_LOCAL_PRESSURE_BOUNDARY_3D_H
#define SET_LOCAL_PRESSURE_BOUNDARY_3D_H

#include <vector>
#include "utilities/functorPtr.h"
#include "geometry/superGeometry3D.h"
#include "extendedFiniteDifferenceBoundary3D.h"
#include "core/superLattice3D.h"
#include "functors/lattice/indicator/superIndicatorBaseF3D.h"
#include "dynamics/dynamics.h"
#include "momentaOnBoundaries3D.h"
#include "io/ostreamManager.h"
#include "functors/lattice/indicator/blockIndicatorF3D.h"

namespace olb {
////////// SuperLattice Domain  /////////////////////////////////////////

///Initialising the setLocalPressureBoundary function on the superLattice domain
///Local Boundaries use the RLBdynamics collision operator
template<typename T,typename DESCRIPTOR, class MixinDynamics=RLBdynamics<T,DESCRIPTOR>>
void setLocalPressureBoundary(SuperLattice3D<T, DESCRIPTOR>& sLattice, T omega, SuperGeometry3D<T>& superGeometry, int material);

///Initialising the setLocalPressureBoundary function on the superLattice domain
template<typename T, typename DESCRIPTOR, typename MixinDynamics=RLBdynamics<T,DESCRIPTOR>>
void setLocalPressureBoundary(SuperLattice3D<T, DESCRIPTOR>& sLattice, T omega, FunctorPtr<SuperIndicatorF3D<T>>&& indicator);

////////// BlockLattice Domain  /////////////////////////////////////////
/// Set pressure boundary for any indicated cells inside the block domain
template<typename T, typename DESCRIPTOR, typename MixinDynamics>
void setLocalPressureBoundary(BlockLatticeStructure3D<T,DESCRIPTOR>& _block, T omega, BlockIndicatorF3D<T>& indicator, bool includeOuterCells);
}


#endif