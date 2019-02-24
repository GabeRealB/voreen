/*  Lattice Boltzmann sample, written in C++, using the OpenLB
 *  library
 *
 *  Copyright (C) 2011-2014 Mathias J. Krause
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


#include "olb3D.h"
#ifndef OLB_PRECOMPILED // Unless precompiled version is used,
#include "olb3D.hh"   // include full template code;
#endif

#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>

using namespace olb;
using namespace olb::descriptors;
using namespace olb::graphics;
using namespace olb::util;

typedef double T;
#define DESCRIPTOR D3Q19Descriptor

enum FlowDirection {
    NONE = -1,
    IN   =  0,
    OUT  =  1,
};

// Indicates flux through an arbitrary, circle-shaped area.
// This code is adapted from the voreen host code.
struct FlowIndicator {
    FlowDirection   direction_{NONE};
    T           center_[3]{0.0};
    T           normal_[3]{0.0};
    T              radius_{0.0};
};

////////// Globals //////////////////
// Meta
std::string simulation = "default";

// Config
T simulationTime = 0.0;
T temporalResolution = 0.0;
int spatialResolution = 1;
std::vector<FlowIndicator> flowIndicators;

// Parameters
T characteristicLength = 0.0;
T characteristicVelocity = 0.0;
T viscosity = 0.0;
T density = 0.0;
bool bouzidiOn = false;
//////////////////////////////////////


// Stores data from stl file in geometry in form of material numbers
void prepareGeometry(UnitConverter<T, DESCRIPTOR> const& converter, IndicatorF3D<T>& indicator,
                     STLreader<T>& stlReader, SuperGeometry3D<T>& superGeometry) {

    OstreamManager clout(std::cout, "prepareGeometry");
    clout << "Prepare Geometry ..." << std::endl;

    superGeometry.rename(0, 2, indicator);
    superGeometry.rename(2, 1, stlReader);

    superGeometry.clean();

    int materialId = 3; // 0=empty, 1=liquid, 2=walls

    for (size_t i = 0; i < flowIndicators.size(); i++) {

        const T* center = flowIndicators[i].center_;
        const T* normal = flowIndicators[i].normal_;
        T radius = flowIndicators[i].radius_;

        // Set material number for inflow
        IndicatorCircle3D<T> inflow(center[0], center[1], center[2], normal[0], normal[1], normal[2], radius);
        IndicatorCylinder3D<T> layerInflow(inflow, 2. * converter.getConversionFactorLength());
        superGeometry.rename(2, materialId, 1, layerInflow);
        materialId++;
    }

    // Removes all not needed boundary voxels outside the surface
    superGeometry.clean();
    // Removes all not needed boundary voxels inside the surface
    superGeometry.innerClean(3);
    superGeometry.checkForErrors();

    superGeometry.print();
    clout << "Prepare Geometry ... OK" << std::endl;
}

// Set up the geometry of the simulation
void prepareLattice(SuperLattice3D<T, DESCRIPTOR>& lattice,
                    UnitConverter<T, DESCRIPTOR> const& converter, Dynamics<T, DESCRIPTOR>& bulkDynamics,
                    sOnLatticeBoundaryCondition3D<T, DESCRIPTOR>& bc,
                    sOffLatticeBoundaryCondition3D<T, DESCRIPTOR>& offBc,
                    STLreader<T>& stlReader, SuperGeometry3D<T>& superGeometry) {

    OstreamManager clout(std::cout, "prepareLattice");
    clout << "Prepare Lattice ..." << std::endl;

    const T omega = converter.getLatticeRelaxationFrequency();

    // material=0 --> do nothing
    lattice.defineDynamics(superGeometry, 0, &instances::getNoDynamics<T, DESCRIPTOR>());

    // material=1 --> bulk dynamics
    lattice.defineDynamics(superGeometry, 1, &bulkDynamics);

    if (bouzidiOn) {
        // material=2 --> no dynamics + bouzidi zero velocity
        lattice.defineDynamics(superGeometry, 2, &instances::getNoDynamics<T, DESCRIPTOR>());
        offBc.addZeroVelocityBoundary(superGeometry, 2, stlReader);
        // material=3 --> no dynamics + bouzidi velocity (inflow)
        lattice.defineDynamics(superGeometry, 3, &instances::getNoDynamics<T, DESCRIPTOR>());
        offBc.addVelocityBoundary(superGeometry, 3, stlReader);
    } else {
        // material=2 --> bounceBack dynamics
        lattice.defineDynamics(superGeometry, 2, &instances::getBounceBack<T, DESCRIPTOR>());
        // material=3 --> bulk dynamics + velocity (inflow)
        lattice.defineDynamics(superGeometry, 3, &bulkDynamics);
        bc.addVelocityBoundary(superGeometry, 3, omega);
    }

    // material=4,5 --> bulk dynamics + pressure (outflow)
    lattice.defineDynamics(superGeometry, 4, &bulkDynamics);
    lattice.defineDynamics(superGeometry, 5, &bulkDynamics);
    bc.addPressureBoundary(superGeometry, 4, omega);
    bc.addPressureBoundary(superGeometry, 5, omega);

    // Initial conditions
    AnalyticalConst3D<T, T> rhoF(1);
    std::vector<T> velocity(3, T());
    AnalyticalConst3D<T, T> uF(velocity);

    // Initialize all values of distribution functions to their local equilibrium
    lattice.defineRhoU(superGeometry, 1, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, 1, rhoF, uF);
    lattice.defineRhoU(superGeometry, 3, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, 3, rhoF, uF);
    lattice.defineRhoU(superGeometry, 4, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, 4, rhoF, uF);
    lattice.defineRhoU(superGeometry, 5, rhoF, uF);
    lattice.iniEquilibrium(superGeometry, 5, rhoF, uF);

    // Lattice initialize
    lattice.initialize();

    clout << "Prepare Lattice ... OK" << std::endl;
}

// Generates a slowly increasing sinuidal inflow
void setBoundaryValues(SuperLattice3D<T, DESCRIPTOR>& sLattice,
                       sOffLatticeBoundaryCondition3D<T, DESCRIPTOR>& offBc,
                       UnitConverter<T, DESCRIPTOR> const& converter, int iT,
                       SuperGeometry3D<T>& superGeometry) {

    // No of time steps for smooth start-up
    int iTperiod = converter.getLatticeTime(0.5);
    int iTupdate = 50;

    if (iT % iTupdate == 0) {
        // Smooth start curve, sinus
        SinusStartScale<T, int> nSinusStartScale(iTperiod, converter.getCharLatticeVelocity());

        // Creates and sets the Poiseuille inflow profile using functors
        int iTvec[1] = {iT};
        T maxVelocity[1] = {T()};
        nSinusStartScale(maxVelocity, iTvec);
        CirclePoiseuille3D<T> velocity(superGeometry, 3, maxVelocity[0]);

        if (bouzidiOn) {
            offBc.defineU(superGeometry, 3, velocity);
        } else {
            sLattice.defineU(superGeometry, 3, velocity);
        }
    }
}

// Computes flux at inflow and outflow
void getResults(SuperLattice3D<T, DESCRIPTOR>& sLattice,
                UnitConverter<T, DESCRIPTOR>& converter, int iT,
                Dynamics<T, DESCRIPTOR>& bulkDynamics,
                SuperGeometry3D<T>& superGeometry, Timer<T>& timer, STLreader<T>& stlReader) {

    OstreamManager clout(std::cout, "getResults");

    SuperVTMwriter3D<T> vtmWriter(simulation.c_str());
    SuperLatticePhysVelocity3D<T, DESCRIPTOR> velocity(sLattice, converter);
    SuperLatticePhysPressure3D<T, DESCRIPTOR> pressure(sLattice, converter);
    vtmWriter.addFunctor(velocity);
    vtmWriter.addFunctor(pressure);

    const int vtkIter = converter.getLatticeTime(.1);
    const int statIter = converter.getLatticeTime(.1);

    if (iT == 0) {
        // Writes the geometry, cuboid no. and rank no. as vti file for visualization
        SuperLatticeGeometry3D<T, DESCRIPTOR> geometry(sLattice, superGeometry);
        SuperLatticeCuboid3D<T, DESCRIPTOR> cuboid(sLattice);
        SuperLatticeRank3D<T, DESCRIPTOR> rank(sLattice);
        vtmWriter.write(geometry);
        vtmWriter.write(cuboid);
        vtmWriter.write(rank);

        vtmWriter.createMasterFile();
    }

    // Writes the vtk files
    if (iT % vtkIter == 0) {
        vtmWriter.write(iT);

        SuperEuklidNorm3D<T, DESCRIPTOR> normVel(velocity);
        BlockReduction3D2D<T> planeReduction(normVel, {0, 0, 1}, 600, BlockDataSyncMode::ReduceOnly);
        // write output as JPEG
        heatmap::write(planeReduction, iT);
    }

    // Writes output on the console
    if (iT % statIter == 0) {
        // Timer console output
        timer.update(iT);
        timer.printStep();

        // Lattice statistics console output
        sLattice.getStatistics().print(iT, converter.getPhysTime(iT));

        if (bouzidiOn) {
            SuperLatticeYplus3D<T, DESCRIPTOR> yPlus(sLattice, converter, superGeometry, stlReader, 3);
            SuperMax3D<T> yPlusMaxF(yPlus, superGeometry, 1);
            int input[4] = {};
            T yPlusMax[1];
            yPlusMaxF(yPlusMax, input);
            clout << "yPlusMax=" << yPlusMax[0] << std::endl;
        }
    }

    if (sLattice.getStatistics().getMaxU() > 0.3) {
        clout << "PROBLEM uMax=" << sLattice.getStatistics().getMaxU() << std::endl;
        vtmWriter.write(iT);
        std::exit(0);
    }
}

int main(int argc, char* argv[]) {

    if(argc != 3) {
        std::cout << "Invalid number of arguments!" << std::endl;
        return EXIT_FAILURE;
    }

    //std::string simulation = argv[0];
    std::string ensemble = argv[1];
    std::string run = argv[2];

    std::cout << "Running: " << simulation << std::endl;
    std::cout << "Ensemble:" << ensemble << std::endl;
    std::cout << "Run: " << run << std::endl;

    // === 0th Step: Create output directory.
    __mode_t mode = ACCESSPERMS;
    std::string output = "/scratch/tmp/s_leis06/simulations/";
    output += simulation + "/";
    mkdir(output.c_str(), mode);
    output += ensemble + "/";
    mkdir(output.c_str(), mode);
    output += run + "/";
    if(mkdir(output.c_str(), mode) != 0) {
        std::cout << "Could not create output directory!" << std::endl;
        return EXIT_FAILURE;
    }

    // === 1st Step: Initialization ===
    singleton::directories().setOutputDir(output.c_str());
    olbInit(&argc, &argv);
    OstreamManager clout(std::cout, "main");
    // don't display messages from every single mpi process
    clout.setMultiOutput(false);

    XMLreader config("config.xml");
    simulationTime = std::atof(config["simulationTime"].getAttribute("value").c_str());
    temporalResolution = std::atof(config["temporalResolution"].getAttribute("value").c_str());
    spatialResolution = std::atoi(config["spatialResolution"].getAttribute("value").c_str());

    XMLreader parameters = config["flowParameters"];
    characteristicLength = std::atof(parameters["characteristicLength"].getAttribute("value").c_str());
    characteristicVelocity = std::atof(parameters["characteristicVelocity"].getAttribute("value").c_str());
    viscosity = std::atof(parameters["viscosity"].getAttribute("value").c_str());
    density = std::atof(parameters["density"].getAttribute("value").c_str());
    bouzidiOn = parameters["bouzidi"].getAttribute("value") == "true";

    XMLreader indicators = config["flowIndicators"];
    for(auto iter : indicators) {
        FlowIndicator indicator;
        indicator.direction_ = static_cast<FlowDirection>(std::atoi((*iter)["direction"].getAttribute("value").c_str()));
        indicator.center_[0] = std::atof((*iter)["center"].getAttribute("x").c_str());
        indicator.center_[1] = std::atof((*iter)["center"].getAttribute("y").c_str());
        indicator.center_[2] = std::atof((*iter)["center"].getAttribute("z").c_str());
        indicator.normal_[0] = std::atof((*iter)["normal"].getAttribute("x").c_str());
        indicator.normal_[1] = std::atof((*iter)["normal"].getAttribute("y").c_str());
        indicator.normal_[2] = std::atof((*iter)["normal"].getAttribute("z").c_str());
        indicator.radius_ = std::atof((*iter)["radius"].getAttribute("value").c_str());
        flowIndicators.push_back(indicator);
    }

    const int N = spatialResolution;
    UnitConverter<T, DESCRIPTOR> converter(
            (T) characteristicLength / N,  // physDeltaX: spacing between two lattice cells in __m__
            (T) temporalResolution,        // physDeltaT: time step in __s__
            (T) characteristicLength,      // charPhysLength: reference length of simulation geometry
            (T) characteristicVelocity,    // charPhysVelocity: maximal/highest expected velocity during simulation in __m / s__
            (T) viscosity,                 // physViscosity: physical kinematic viscosity in __m^2 / s__
            (T) density                    // physDensity: physical density in __kg / m^3__
    );
    // Prints the converter log as console output
    converter.print();
    // Writes the converter log in a file
    converter.write(simulation.c_str());

    // === 2nd Step: Prepare Geometry ===

    // Instantiation of the STLreader class
    // file name, voxel size in meter, stl unit in meter, outer voxel no., inner voxel no.
    std::string geometryFileName = "../geometry/geometry.stl";
    STLreader<T> stlReader(geometryFileName.c_str(), converter.getConversionFactorLength(), 1.0f, 0, true);
    IndicatorLayer3D<T> extendedDomain(stlReader, converter.getConversionFactorLength());

    // Instantiation of a cuboidGeometry with weights
#ifdef PARALLEL_MODE_MPI
    const int noOfCuboids = std::min( 16*N,2*singleton::mpi().getSize() );
#else
    const int noOfCuboids = 2;
#endif
    CuboidGeometry3D<T> cuboidGeometry(extendedDomain, converter.getConversionFactorLength(), noOfCuboids);

    // Instantiation of a loadBalancer
    HeuristicLoadBalancer<T> loadBalancer(cuboidGeometry);

    // Instantiation of a superGeometry
    SuperGeometry3D<T> superGeometry(cuboidGeometry, loadBalancer, 2);

    prepareGeometry(converter, extendedDomain, stlReader, superGeometry);

    // === 3rd Step: Prepare Lattice ===
    SuperLattice3D<T, DESCRIPTOR> sLattice(superGeometry);

    SmagorinskyBGKdynamics<T, DESCRIPTOR> bulkDynamics(converter.getLatticeRelaxationFrequency(),
                                                       instances::getBulkMomenta<T, DESCRIPTOR>(), 0.1);

    // choose between local and non-local boundary condition
    sOnLatticeBoundaryCondition3D<T, DESCRIPTOR> sBoundaryCondition(sLattice);
    createInterpBoundaryCondition3D<T, DESCRIPTOR>(sBoundaryCondition);
    // createLocalBoundaryCondition3D<T,DESCRIPTOR>(sBoundaryCondition);

    sOffLatticeBoundaryCondition3D<T, DESCRIPTOR> sOffBoundaryCondition(sLattice);
    createBouzidiBoundaryCondition3D<T, DESCRIPTOR>(sOffBoundaryCondition);

    Timer<T> timer1(converter.getLatticeTime(simulationTime), superGeometry.getStatistics().getNvoxel());
    timer1.start();

    prepareLattice(sLattice, converter, bulkDynamics,
                   sBoundaryCondition, sOffBoundaryCondition,
                   stlReader, superGeometry);

    timer1.stop();
    timer1.printSummary();

    // === 4th Step: Main Loop with Timer ===
    clout << "starting simulation..." << std::endl;
    Timer<T> timer(converter.getLatticeTime(simulationTime), superGeometry.getStatistics().getNvoxel());
    timer.start();

    for (int iT = 0; iT <= converter.getLatticeTime(simulationTime); iT++) {

        // === 5th Step: Definition of Initial and Boundary Conditions ===
        setBoundaryValues(sLattice, sOffBoundaryCondition, converter, iT, superGeometry);

        // === 6th Step: Collide and Stream Execution ===
        sLattice.collideAndStream();

        // === 7th Step: Computation and Output of the Results ===
        getResults(sLattice, converter, iT, bulkDynamics, superGeometry, timer, stlReader);
    }

    timer.stop();
    timer.printSummary();
}
