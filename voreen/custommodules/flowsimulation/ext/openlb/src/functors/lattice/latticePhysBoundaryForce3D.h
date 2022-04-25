/*  This file is part of the OpenLB library
 *
 *  Copyright (C) 2012 Lukas Baron, Tim Dornieden, Mathias J. Krause,
 *  Albert Mink
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

#ifndef LATTICE_PHYS_BOUNDARY_FORCE_3D_H
#define LATTICE_PHYS_BOUNDARY_FORCE_3D_H

#include<vector>

#include "superBaseF3D.h"
#include "superCalcF3D.h"
#include "functors/analytical/indicator/indicatorBaseF3D.h"

#include "blockBaseF3D.h"
#include "geometry/blockGeometry.h"
#include "functors/analytical/indicator/indicatorBaseF3D.h"
#include "indicator/blockIndicatorBaseF3D.h"
#include "dynamics/smagorinskyBGKdynamics.h"
#include "dynamics/porousBGKdynamics.h"


/* Note: Throughout the whole source code directory genericFunctions, the
 *  template parameters for i/o dimensions are:
 *           F: S^m -> T^n  (S=source, T=target)
 */

namespace olb {

/// functor to get pointwise phys force acting on a boundary with a given material on local lattice
template <typename T, typename DESCRIPTOR>
class SuperLatticePhysBoundaryForce3D : public SuperLatticePhysF3D<T,DESCRIPTOR> {
private:
  FunctorPtr<SuperIndicatorF3D<T>> _indicatorF;
public:
  SuperLatticePhysBoundaryForce3D(SuperLattice<T,DESCRIPTOR>&      sLattice,
                                  FunctorPtr<SuperIndicatorF3D<T>>&& indicatorF,
                                  const UnitConverter<T,DESCRIPTOR>& converter);
  SuperLatticePhysBoundaryForce3D(SuperLattice<T,DESCRIPTOR>& sLattice,
                                  SuperGeometry<T,3>& superGeometry, const int material,
                                  const UnitConverter<T,DESCRIPTOR>& converter);
};

/// functor returns pointwise phys force acting on a boundary with a given material on local lattice
template <typename T, typename DESCRIPTOR>
class BlockLatticePhysBoundaryForce3D final : public BlockLatticePhysF3D<T,DESCRIPTOR> {
private:
  BlockIndicatorF3D<T>&        _indicatorF;
  BlockGeometry<T,3>& _blockGeometry;
public:
  BlockLatticePhysBoundaryForce3D(BlockLattice<T,DESCRIPTOR>& blockLattice,
                                  BlockIndicatorF3D<T>& indicatorF,
                                  const UnitConverter<T,DESCRIPTOR>& converter);
  bool operator() (T output[], const int input[]) override;
};

}
#endif
