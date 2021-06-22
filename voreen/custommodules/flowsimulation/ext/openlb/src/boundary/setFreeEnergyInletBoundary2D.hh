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

///This file contains the Free Energy Inlet Boundary
///This is a new version of the Boundary, which only contains free floating functions
#ifndef SET_FREE_ENERGY_INLET_BOUNDARY_2D_HH
#define SET_FREE_ENERGY_INLET_BOUNDARY_2D_HH

#include "setFreeEnergyInletBoundary2D.h"

namespace olb {

///Initialising the Free Energy Inlet Boundary on the superLattice domain
template<typename T, typename DESCRIPTOR, typename MixinDynamics>
void setFreeEnergyInletBoundary(SuperLattice2D<T, DESCRIPTOR>& sLattice, T omega, SuperGeometry2D<T>& superGeometry, int material, std::string type, int latticeNumber)
{
  setFreeEnergyInletBoundary<T,DESCRIPTOR, MixinDynamics>(sLattice, omega, superGeometry.getMaterialIndicator(material), type, latticeNumber);

}

///Initialising the Free Energy Inlet Boundary on the superLattice domain
template<typename T, typename DESCRIPTOR, typename MixinDynamics>
void setFreeEnergyInletBoundary(SuperLattice2D<T, DESCRIPTOR>& sLattice, T omega, FunctorPtr<SuperIndicatorF2D<T>>&& indicator, std::string type, int latticeNumber)
{
  OstreamManager clout(std::cout, "setFreeEnergyInletBoundary");
  bool includeOuterCells = false;
  /*  local boundaries: _overlap = 0;
   *  interp boundaries: _overlap = 1;
   *  bouzidi boundaries: _overlap = 1;
   *  extField boundaries: _overlap = 1;
   *  advectionDiffusion boundaries: _overlap = 1;
   */
  int _overlap = 1;
  if (indicator->getSuperGeometry().getOverlap() == 1) {
    includeOuterCells = true;
    clout << "WARNING: overlap == 1, boundary conditions set on overlap despite unknown neighbor materials" << std::endl;
  }
  for (int iCloc = 0; iCloc < sLattice.getLoadBalancer().size(); ++iCloc) {
    setFreeEnergyInletBoundary<T,DESCRIPTOR, MixinDynamics>(sLattice.getExtendedBlockLattice(iCloc), omega,
        indicator->getExtendedBlockIndicatorF(iCloc), type, latticeNumber, includeOuterCells);
  }
  /// Adds needed Cells to the Communicator _commBC in SuperLattice
  addPoints2CommBC<T,DESCRIPTOR>(sLattice, std::forward<decltype(indicator)>(indicator), _overlap);

}

/// Set Free Energy Inlet boundary for any indicated cells inside the block domain
template<typename T, typename DESCRIPTOR, typename MixinDynamics>
void setFreeEnergyInletBoundary(BlockLatticeStructure2D<T,DESCRIPTOR>& block,T omega, BlockIndicatorF2D<T>& indicator, std::string type,
                                int latticeNumber, bool includeOuterCells)
{
  bool _output = false;
  OstreamManager clout(std::cout, "setFreeEnergyInletBoundary");
  auto& blockGeometryStructure = indicator.getBlockGeometryStructure();
  const int margin = includeOuterCells ? 0 : 1;
  /*
  *x0,x1,y0,y1 Range of cells to be traversed
  **/
  int x0 = margin;
  int y0 = margin;
  int x1 = blockGeometryStructure.getNx()-1 -margin;
  int y1 = blockGeometryStructure.getNy()-1 -margin;
  std::vector<int> discreteNormal(3, 0);
  for (int iX = x0; iX <= x1; ++iX) {
    for (int iY = y0; iY <= y1; ++iY) {
      if (indicator(iX,iY)) {
        discreteNormal = blockGeometryStructure.getStatistics().getType(iX,iY);
        if (discreteNormal[0] == 0) {

          Momenta<T, DESCRIPTOR>* momenta = nullptr;
          Dynamics<T, DESCRIPTOR>* dynamics = NULL;
          if (discreteNormal[1] == -1) {
            if (latticeNumber == 1) {
              //set momenta and dynamics for a pressure boundary on indicated cells
              if (type == "density") {
                //momenta vector provisionally inside src/core/blockLatticeStructure3D.h
                momenta = new RegularizedPressureBM<T,DESCRIPTOR, 0,-1>;
                //dynamics vector provisionally inside src/core/blockLatticeStructure3D.h
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              else {
                //set momenta and dynamics for a velocity boundary on indicated cells
                momenta = new RegularizedVelocityBM<T,DESCRIPTOR, 0,-1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);

              }
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
            else {
              momenta = new RegularizedPressureBM<T,DESCRIPTOR, 0,-1>;
              dynamics = new FreeEnergyInletOutletDynamics<T,DESCRIPTOR,0,-1>(omega,*momenta);
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
          }

          else if (discreteNormal[1] == 1) {
            if (latticeNumber == 1) {
              if (type == "density") {
                //set momenta and dynamics for a pressure boundary on indicated cells
                momenta = new RegularizedPressureBM<T,DESCRIPTOR, 0,1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              else {
                //set momenta and dynamics for a velocity boundary on indicated cells
                momenta = new RegularizedVelocityBM<T,DESCRIPTOR, 0,1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              block.defineDynamics(iX, iX, iY, iY, dynamics);

            }
            else {
              momenta = new RegularizedPressureBM<T,DESCRIPTOR, 0,1>;
              dynamics = new FreeEnergyInletOutletDynamics<T,DESCRIPTOR,0,1>(omega,*momenta);
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
          }

          else if (discreteNormal[2] == -1) {
            if (latticeNumber == 1) {
              if (type == "density") {
                //set momenta and dynamics for a pressure boundary on indicated cells
                momenta = new RegularizedPressureBM<T,DESCRIPTOR, 1,-1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              else {
                //set momenta and dynamics for a velocity boundary on indicated cells
                momenta = new RegularizedVelocityBM<T,DESCRIPTOR, 1,-1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);

              }
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
            else {
              momenta = new RegularizedPressureBM<T,DESCRIPTOR, 1,-1>;
              dynamics = new FreeEnergyInletOutletDynamics<T,DESCRIPTOR,1,-1>(omega,*momenta);
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
          }

          else if (discreteNormal[2] == 1) {
            if (latticeNumber == 1) {
              if (type == "density") {
                //set momenta and dynamics for a pressure boundary on indicated cells
                momenta = new RegularizedPressureBM<T,DESCRIPTOR, 1,1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              else {
                //set momenta and dynamics for a velocity boundary on indicated cells
                momenta = new RegularizedVelocityBM<T,DESCRIPTOR, 1,1>;
                dynamics = new CombinedRLBdynamics<T,DESCRIPTOR, MixinDynamics>(omega, *momenta);
              }
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
            else {
              momenta = new RegularizedPressureBM<T,DESCRIPTOR, 1,1>;
              dynamics = new FreeEnergyInletOutletDynamics<T,DESCRIPTOR,1,1>(omega,*momenta);
              block.defineDynamics(iX, iX, iY, iY, dynamics);
            }
          }

          if (latticeNumber != 1) {
            block.get(iX,iY).defineDynamics(dynamics);
            block.momentaVector.push_back(momenta);
            block.dynamicsVector.push_back(dynamics);
          }
          PostProcessorGenerator2D<T, DESCRIPTOR>* postProcessor = new FreeEnergyChemPotBoundaryProcessorGenerator2D<T, DESCRIPTOR> (
            iX, iX, iY, iY, discreteNormal[1], discreteNormal[2], latticeNumber );
          //sets the boundary on any indicated cell
          //located in setLocalVelocityBoundary2D.h/hh
          setBoundary<T, DESCRIPTOR, MixinDynamics>(block,  omega, iX,iY, momenta, dynamics, postProcessor);


          if (_output) {
            clout << "setFreeEnergyInletBoundary<" << "," << ">("  << x0 << ", "<< x1 << ", " << y0 << ", " << y1 << ", " << " )" << std::endl;
          }

        }
      }
    }
  }
}

}//namespace olb
#endif
