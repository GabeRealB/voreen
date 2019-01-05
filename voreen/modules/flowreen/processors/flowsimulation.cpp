/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany,                        *
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

#include "flowsimulation.h"

#include "voreen/core/datastructures/geometry/glmeshgeometry.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"
#include "voreen/core/datastructures/volume/volumefactory.h"
#include "voreen/core/datastructures/volume/volumeminmaxmagnitude.h"

#include "modules/hdf5/io/hdf5volumereader.h"
#include "modules/hdf5/io/hdf5volumewriter.h"

#include "modules/flowreen/utils/geometryconverter.h"

namespace voreen {

const std::string FlowSimulation::loggerCat_("voreen.flowreen.FlowSimulation");

FlowSimulation::FlowSimulation()
    : AsyncComputeProcessor<ComputeInput, ComputeOutput>()
    // ports
    , geometryDataPort_(Port::INPORT, "geometryDataPort", "Geometry Input", false)
    , measuredDataPort_(Port::INPORT, "measuredDataPort", "Measured Data Input", false)
    , outport_(Port::OUTPORT, "outport", "Time Series Output")
    , simulationTime_("simulationTime", "Simulation Time (s)", 2.0f, 0.1f, 10.0f)
    , temporalResolution_("temporalResolution", "Temporal Resolution (ms)", 3.1f, 1.0f, 30.0f)
    , characteristicLength_("characteristicLength", "Characteristic Length (mm)", 22.46f, 1.0f, 100.0f)
    , viscosity_("viscosity", "Viscosity (e-6 m^2/s)", 3.5, 3, 4)
    , density_("density", "Density (kg/m^3)", 1000.0f, 1000.0f, 1100.0f)
    , bouzidi_("bounzidi", "Bounzidi", true)
{
    addPort(geometryDataPort_);
    addPort(measuredDataPort_);
    addPort(outport_);

    addProperty(simulationTime_);
    addProperty(temporalResolution_);
    addProperty(characteristicLength_);
    addProperty(viscosity_);
    addProperty(density_);
    addProperty(bouzidi_);
}

FlowSimulation::~FlowSimulation() {
}

bool FlowSimulation::isReady() const {
    if(!isInitialized()) {
        setNotReadyErrorMessage("Not initialized.");
        return false;
    }
    if(!geometryDataPort_.isReady()) {
        setNotReadyErrorMessage("Geometry Port not ready.");
        return false;
    }
    // Note: measuredDataPort ist optional!
    return true;
}

FlowSimulationInput FlowSimulation::prepareComputeInput() {
    const Geometry* geometryData = geometryDataPort_.getData();
    if (!geometryData) {
        throw InvalidInputException("No input", InvalidInputException::S_WARNING);
    }

    const VolumeList* measuredData = measuredDataPort_.getData();
    if(!measuredData || measuredData->empty()) {
        throw InvalidInputException("Unsteered simulations currently not supported", InvalidInputException::S_ERROR);
    }

    LINFO("Configuring a steered simulation");

    // TODO: create new data-/port- type which resamples all contained volume into a cube or at least performs the checks below.
    // Check for volume compatibility
    VolumeBase* volumeT0 = measuredData->first();
    // Currently only 3xFloat Volumes are considered. This condition could be relaxed in the future.
    if(volumeT0->getFormat() != VolumeGenerator3xFloat().getFormat()) {
        throw InvalidInputException("Measured data contains volume different from 3xFloat", InvalidInputException::S_ERROR);
    }
    if(volumeT0->getDimensions() != tgt::svec3(volumeT0->getDimensions().x)) {
        throw InvalidInputException("Measured data must have dimensions: n x n x n", InvalidInputException::S_ERROR);
    }
    if(volumeT0->getSpacing() != tgt::vec3(volumeT0->getSpacing().x)) {
        throw InvalidInputException("Measured data must have spacing: n x n x n", InvalidInputException::S_ERROR);
    }

    if(!volumeT0->hasDerivedData<VolumeMinMaxMagnitude>()) {
        LWARNING("Calculating VolumeMinMaxMagnitude. This may take a while...");
        //throw InvalidInputException("VolumeMinMaxMagnitude not available!", InvalidInputException::S_WARNING);
    }

    float minVelocityMagnitude = volumeT0->getDerivedData<VolumeMinMaxMagnitude>()->getMinMagnitude();
    float maxVelocityMagnitude = volumeT0->getDerivedData<VolumeMinMaxMagnitude>()->getMaxMagnitude();

    for(size_t i=1; i<measuredData->size(); i++) {
        VolumeBase* volumeTi = measuredData->at(i);
        if(volumeT0->getFormat() != volumeTi->getFormat()
            || volumeT0->getDimensions() != volumeTi->getDimensions()
            || volumeT0->getSpacing() != volumeTi->getSpacing()) {
            throw InvalidInputException("Measured data contains different kinds of volumes.", InvalidInputException::S_ERROR);
        }

        minVelocityMagnitude = std::min(minVelocityMagnitude, volumeTi->getDerivedData<VolumeMinMaxMagnitude>()->getMinMagnitude());
        maxVelocityMagnitude = std::min(maxVelocityMagnitude, volumeTi->getDerivedData<VolumeMinMaxMagnitude>()->getMaxMagnitude());
    }

    // === 1st Step: Initialization ===
    UnitConverter<T,DESCRIPTOR> converter(
            (T)   volumeT0->getSpacing().x,                 // physDeltaX: spacing between two lattice cells in __m__
            (T)   temporalResolution_.get(),                // physDeltaT: time step in __s__
            (T)   characteristicLength_.get(),              // charPhysLength: reference length of simulation geometry
            (T)   maxVelocityMagnitude/1000.0,              // charPhysVelocity: maximal/highest expected velocity during simulation in __m / s__
            (T)   viscosity_.get()/1e-10,                   // physViscosity: physical kinematic viscosity in __m^2 / s__
            (T)   density_.get()                            // physDensity: physical density in __kg / m^3__
    );

    // Prints the converter log as console output
    converter.print();
    // Writes the converter log in a file
    converter.write("aorta3d");

    // === 2nd Step: Prepare Geometry ===

    // Instantiation of the STLreader class
    // file name, voxel size in meter, stl unit in meter, outer voxel no., inner voxel no.
    std::unique_ptr<STLreader<T>> stlReader = convertGeometryToSTL(geometryData);
    if(!stlReader) {
        throw InvalidInputException("Geometry could not be initialized", InvalidInputException::S_ERROR);
    }

/*
    const std::string volumeLocation = HDF5VolumeWriter::VOLUME_DATASET_NAME;
    const std::string baseType = volumeT0->getBaseType();
    const tgt::svec3 outputDim = volumeT0->getDimensions();
    const int deflateLevel = 1;

    std::vector<std::unique_ptr<HDF5FileVolume>> outputVolumes;
    for(size_t i = 0; i<10; i++) {
        std::unique_ptr<HDF5FileVolume> outputVolume = nullptr;
        try {
            outputVolume = std::unique_ptr<HDF5FileVolume>(
                    HDF5FileVolume::createVolume(volumeFilePath, volumeLocation, baseType, outputDim, 1, true,
                                                 deflateLevel, tgt::svec3(outputDim.xy(), 1), false));
        } catch (tgt::IOException e) {
            throw InvalidInputException("Could not create output volume.", InvalidInputException::S_ERROR);
        }

        outputVolume->writeSpacing(volumeT0->getSpacing());
        outputVolume->writeOffset(volumeT0->getOffset());
        outputVolume->writePhysicalToWorldTransformation(volumeT0->getPhysicalToWorldMatrix());
        outputVolume->writeRealWorldMapping(volumeT0->getRealWorldMapping());
        outputVolumes.emplace_back(std::move(outputDim));
    }
*/
    return FlowSimulationInput{
            simulationTime_.get(),
            converter,
            std::move(stlReader),
            //outputVolumes
    };
}

FlowSimulationOutput FlowSimulation::compute(FlowSimulationInput input, ProgressReporter& progressReporter) const {

    progressReporter.setProgress(0.0f);

    std::unique_ptr<VolumeList> output = nullptr;//std::move(input.outputVolumes);

    // Needs to be initialized in each new thread to be used.
    olb::olbInit(nullptr, nullptr);

    IndicatorLayer3D<T> extendedDomain( *input.stlReader, input.converter.getConversionFactorLength() );

    // Instantiation of a cuboidGeometry with weights
#ifdef PARALLEL_MODE_MPI
    const int noOfCuboids = std::min( 16*N,2*singleton::mpi().getSize() );
#else
    const int noOfCuboids = 2;
#endif
    CuboidGeometry3D<T> cuboidGeometry( extendedDomain, input.converter.getConversionFactorLength(), noOfCuboids );

    // Instantiation of a loadBalancer
    HeuristicLoadBalancer<T> loadBalancer( cuboidGeometry );

    // Instantiation of a superGeometry
    SuperGeometry3D<T> superGeometry( cuboidGeometry, loadBalancer, 2 );

    prepareGeometry( input.converter, extendedDomain, *input.stlReader, superGeometry );

    // === 3rd Step: Prepare Lattice ===
    SuperLattice3D<T, DESCRIPTOR> sLattice(superGeometry);

    SmagorinskyBGKdynamics<T, DESCRIPTOR> bulkDynamics(
            input.converter.getLatticeRelaxationFrequency(),
            instances::getBulkMomenta<T, DESCRIPTOR>(), 0.1 );

    // choose between local and non-local boundary condition
    sOnLatticeBoundaryCondition3D<T,DESCRIPTOR> sBoundaryCondition( sLattice );
    createInterpBoundaryCondition3D<T,DESCRIPTOR>( sBoundaryCondition );
    // createLocalBoundaryCondition3D<T,DESCRIPTOR>(sBoundaryCondition);

    sOffLatticeBoundaryCondition3D<T, DESCRIPTOR> sOffBoundaryCondition( sLattice );
    createBouzidiBoundaryCondition3D<T, DESCRIPTOR> ( sOffBoundaryCondition );

    prepareLattice( sLattice, input.converter, bulkDynamics,
                    sBoundaryCondition, sOffBoundaryCondition,
                    *input.stlReader, superGeometry );

    // === 4th Step: Main Loop ===
    for ( int iT = 0; iT <= input.converter.getLatticeTime( input.simulationTime ); iT++ ) {

        // === 5th Step: Definition of Initial and Boundary Conditions ===
        setBoundaryValues( sLattice, sOffBoundaryCondition, input.converter, iT, superGeometry );

        // === 6th Step: Collide and Stream Execution ===
        sLattice.collideAndStream();

        // === 7th Step: Computation and Output of the Results ===
        bool success = getResults(sLattice, input.converter, iT, output.get());
        if(!success)
            break;

        float progress = iT / (input.converter.getLatticeTime( input.simulationTime ) + 1.0f);
        progressReporter.setProgress(progress);
    }
    progressReporter.setProgress(1.0f);

    // Done.
    return FlowSimulationOutput{
            //std::move(output)
    };
}

void FlowSimulation::processComputeOutput(FlowSimulationOutput output) {
}

// Stores data from stl file in geometry in form of material numbers
void FlowSimulation::prepareGeometry( UnitConverter<T,DESCRIPTOR> const& converter, IndicatorF3D<T>& indicator,
                                      STLreader<T>& stlReader, SuperGeometry3D<T>& superGeometry ) const {

    LINFO("Prepare Geometry ...");

    superGeometry.rename( 0,2,indicator );
    superGeometry.rename( 2,1,stlReader );

    superGeometry.clean();

    // TODO: these have to be set via Voreen - interface!
    // Set material number for inflow
    IndicatorCircle3D<T> inflow(  0.218125 ,0.249987 ,0.0234818, 0., 1.,0., 0.0112342 );
    IndicatorCylinder3D<T> layerInflow( inflow, 2.*converter.getConversionFactorLength() );
    superGeometry.rename( 2,3,1,layerInflow );

    // Set material number for outflow0
    //IndicatorCircle3D<T> outflow0(0.2053696,0.0900099,0.0346537,  2.5522,5.0294,-1.5237, 0.0054686 );
    IndicatorCircle3D<T> outflow0( 0.2053696,0.0900099,0.0346537, 0.,-1.,0., 0.0054686 );
    IndicatorCylinder3D<T> layerOutflow0( outflow0, 2.*converter.getConversionFactorLength() );
    superGeometry.rename( 2,4,1,layerOutflow0 );

    // Set material number for outflow1
    //IndicatorCircle3D<T> outflow1(0.2388403,0.0900099,0.0343228, -1.5129,5.1039,-2.8431, 0.0058006 );
    IndicatorCircle3D<T> outflow1( 0.2388403,0.0900099,0.0343228, 0.,-1.,0., 0.0058006 );
    IndicatorCylinder3D<T> layerOutflow1( outflow1, 2.*converter.getConversionFactorLength() );
    superGeometry.rename( 2,5,1,layerOutflow1 );

    // Removes all not needed boundary voxels outside the surface
    superGeometry.clean();
    // Removes all not needed boundary voxels inside the surface
    superGeometry.innerClean( 3 );
    superGeometry.checkForErrors();

    LINFO("Prepare Geometry ... OK");
}

// Set up the geometry of the simulation
void FlowSimulation::prepareLattice( SuperLattice3D<T, DESCRIPTOR>& lattice,
                                     UnitConverter<T,DESCRIPTOR> const& converter,
                                     Dynamics<T, DESCRIPTOR>& bulkDynamics,
                                     sOnLatticeBoundaryCondition3D<T, DESCRIPTOR>& bc,
                                     sOffLatticeBoundaryCondition3D<T,DESCRIPTOR>& offBc,
                                     STLreader<T>& stlReader, SuperGeometry3D<T>& superGeometry ) const {

    LINFO("Prepare Lattice ...");

    const T omega = converter.getLatticeRelaxationFrequency();

    // material=0 --> do nothing
    lattice.defineDynamics( superGeometry,0,&instances::getNoDynamics<T, DESCRIPTOR>() );

    // material=1 --> bulk dynamics
    lattice.defineDynamics( superGeometry,1,&bulkDynamics );

    if ( bouzidi_.get() ) {
        // material=2 --> no dynamics + bouzidi zero velocity
        lattice.defineDynamics( superGeometry,2,&instances::getNoDynamics<T,DESCRIPTOR>() );
        offBc.addZeroVelocityBoundary( superGeometry,2,stlReader );
        // material=3 --> no dynamics + bouzidi velocity (inflow)
        lattice.defineDynamics( superGeometry,3,&instances::getNoDynamics<T,DESCRIPTOR>() );
        offBc.addVelocityBoundary( superGeometry,3,stlReader );
    } else {
        // material=2 --> bounceBack dynamics
        lattice.defineDynamics( superGeometry, 2, &instances::getBounceBack<T, DESCRIPTOR>() );
        // material=3 --> bulk dynamics + velocity (inflow)
        lattice.defineDynamics( superGeometry,3,&bulkDynamics );
        bc.addVelocityBoundary( superGeometry,3,omega );
    }

    // material=4,5 --> bulk dynamics + pressure (outflow)
    lattice.defineDynamics( superGeometry,4,&bulkDynamics );
    lattice.defineDynamics( superGeometry,5,&bulkDynamics );
    bc.addPressureBoundary( superGeometry,4,omega );
    bc.addPressureBoundary( superGeometry,5,omega );

    // Initial conditions
    AnalyticalConst3D<T,T> rhoF( 1 );
    std::vector<T> velocity( 3,T() );
    AnalyticalConst3D<T,T> uF( velocity );

    // Initialize all values of distribution functions to their local equilibrium
    lattice.defineRhoU( superGeometry,1,rhoF,uF );
    lattice.iniEquilibrium( superGeometry,1,rhoF,uF );
    lattice.defineRhoU( superGeometry,3,rhoF,uF );
    lattice.iniEquilibrium( superGeometry,3,rhoF,uF );
    lattice.defineRhoU( superGeometry,4,rhoF,uF );
    lattice.iniEquilibrium( superGeometry,4,rhoF,uF );
    lattice.defineRhoU( superGeometry,5,rhoF,uF );
    lattice.iniEquilibrium( superGeometry,5,rhoF,uF );

    // Lattice initialize
    lattice.initialize();

    LINFO("Prepare Lattice ... OK");
}

// Generates a slowly increasing sinuidal inflow
void FlowSimulation::setBoundaryValues( SuperLattice3D<T, DESCRIPTOR>& sLattice,
                                        sOffLatticeBoundaryCondition3D<T,DESCRIPTOR>& offBc,
                                        UnitConverter<T,DESCRIPTOR> const& converter, int iT,
                                        SuperGeometry3D<T>& superGeometry ) const {
    // No of time steps for smooth start-up
    int iTperiod = converter.getLatticeTime( 0.5 );
    int iTupdate = 50;

    if ( iT%iTupdate == 0 ) {
        // Smooth start curve, sinus
        SinusStartScale<T,int> nSinusStartScale( iTperiod,converter.getCharLatticeVelocity() );

        // Creates and sets the Poiseuille inflow profile using functors
        int iTvec[1]= {iT};
        T maxVelocity[1]= {T()};
        nSinusStartScale( maxVelocity,iTvec );
        CirclePoiseuille3D<T> velocity( superGeometry,3,maxVelocity[0] );

        if ( bouzidi_.get() ) {
            offBc.defineU( superGeometry,3,velocity );
        } else {
            sLattice.defineU( superGeometry,3,velocity );
        }
    }
}

// Computes flux at inflow and outflow
bool FlowSimulation::getResults( SuperLattice3D<T, DESCRIPTOR>& sLattice,
                                 UnitConverter<T,DESCRIPTOR>& converter, int iT,
                                 VolumeList* volumeList ) const {
    OstreamManager clout( std::cout,"getResults" );

    SuperVTMwriter3D<T> vtmWriter( "aorta3d" );
    SuperLatticePhysVelocity3D<T, DESCRIPTOR> velocity( sLattice, converter );
    SuperLatticePhysPressure3D<T, DESCRIPTOR> pressure( sLattice, converter );
    vtmWriter.addFunctor( velocity );
    vtmWriter.addFunctor( pressure );

    const int vtkIter  = converter.getLatticeTime( .1 );

    /*
    if ( iT==0 ) {
        // Writes the geometry, cuboid no. and rank no. as vti file for visualization
        SuperLatticeGeometry3D<T, DESCRIPTOR> geometry( sLattice, superGeometry );
        SuperLatticeCuboid3D<T, DESCRIPTOR> cuboid( sLattice );
        SuperLatticeRank3D<T, DESCRIPTOR> rank( sLattice );
        vtmWriter.write( geometry );
        vtmWriter.write( cuboid );
        vtmWriter.write( rank );

        vtmWriter.createMasterFile();
    }
     */

    // Writes the vtk files
    if ( iT%vtkIter==0 ) {
        vtmWriter.write( iT );

        SuperEuklidNorm3D<T, DESCRIPTOR> normVel( velocity );
        BlockReduction3D2D<T> planeReduction( normVel, {0,0,1}, 600, BlockDataSyncMode::ReduceOnly );
        // write output as JPEG
        heatmap::write(planeReduction, iT);
    }

    return true;
}

}   // namespace
