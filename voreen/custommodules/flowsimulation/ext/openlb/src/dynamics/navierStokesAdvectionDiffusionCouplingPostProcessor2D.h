/*  This file is part of the OpenLB library
 *
 *  Copyright (C) 2008 Orestis Malaspinas, Andrea Parmigiani, Jonas Latt
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

#ifndef NAVIER_STOKES_ADVECTION_DIFFUSION_COUPLING_POST_PROCESSOR_2D_H
#define NAVIER_STOKES_ADVECTION_DIFFUSION_COUPLING_POST_PROCESSOR_2D_H

#include "core/spatiallyExtendedObject2D.h"
#include "core/postProcessing.h"
#include "core/blockLattice2D.h"
#include <cmath>


namespace olb {

using namespace descriptors;

/**
* Class for the coupling between a Navier-Stokes (NS) lattice and an
* Advection-Diffusion (AD) lattice.
*/

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!//
// This coupling must be necessarily be put on the Navier-Stokes lattice!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!//

//======================================================================
// ========Regularized NSDiffusion Coupling 2D ====================//
//======================================================================
template<typename T, template<typename U> class Lattice>
class NavierStokesAdvectionDiffusionCouplingPostProcessor2D : public LocalPostProcessor2D<T,Lattice> {
public:
  NavierStokesAdvectionDiffusionCouplingPostProcessor2D(int x0_, int x1_, int y0_, int y1_,
      T gravity_, T T0_, T deltaTemp_, std::vector<T> dir_,
      std::vector<SpatiallyExtendedObject2D* > partners_);
  int extent() const override
  {
    return 0;
  }
  int extent(int whichDirection) const override
  {
    return 0;
  }
  void process(BlockLattice2D<T,Lattice>& blockLattice) override;
  void processSubDomain(BlockLattice2D<T,Lattice>& blockLattice,
                                int x0_, int x1_, int y0_, int y1_) override;
private:
  typedef Lattice<T> L;
  int x0, x1, y0, y1;
  T gravity, T0, deltaTemp;
  std::vector<T> dir;
  BlockLattice2D<T,AdvectionDiffusionD2Q5Descriptor> *tPartner;
  T forcePrefactor[L::d];

  std::vector<SpatiallyExtendedObject2D*> partners;
  enum {
    velOffset = AdvectionDiffusionD2Q5Descriptor<T>::ExternalField::velocityBeginsAt,
    forceOffset = ForcedD2Q9Descriptor<T>::ExternalField::forceBeginsAt
  };
};

template<typename T, template<typename U> class Lattice>
class NavierStokesAdvectionDiffusionCouplingGenerator2D : public LatticeCouplingGenerator2D<T,Lattice> {
public:
  NavierStokesAdvectionDiffusionCouplingGenerator2D(int x0_, int x1_, int y0_, int y1_,
      T gravity_, T T0_, T deltaTemp_, std::vector<T> dir_);
  PostProcessor2D<T,Lattice>* generate(std::vector<SpatiallyExtendedObject2D* > partners) const override;
  LatticeCouplingGenerator2D<T,Lattice>* clone() const override;

private:
  T gravity, T0, deltaTemp;
  std::vector<T> dir;
};


}

#endif