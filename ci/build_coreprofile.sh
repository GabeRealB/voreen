#!/bin/bash

#cd ${VOREEN_ROOT}

# clear build directory (also clears CMake cache)
BUILD_DIR=$1
rm -rf $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

# build configuration
build_options=(
    -DCMAKE_BUILD_TYPE=Debug

    -DVRN_OPENGL_COMPATIBILITY_PROFILE=OFF #required to build some modules

    -DVRN_BUILD_VOREENVE=ON
    -DVRN_BUILD_VOREENTOOL=ON
    -DVRN_BUILD_SIMPLEGLUT=ON
    -DVRN_BUILD_SIMPLEQT=ON
    -DVRN_BUILD_TESTAPPS=ON
    -DVRN_BUILD_BLASTEST=ON
    -DVRN_BUILD_ITKWRAPPER=ON
    #-DVRN_DEPLOYMENT=ON

    # public modules
    -DVRN_MODULE_BASE=ON
    -DVRN_MODULE_CONNEXE=ON
    #-DVRN_MODULE_DYNAMICGLSL=ON #Only compatibility
    #-DVRN_MODULE_EXPERIMENTAL=ON #Only compatibility
    -DVRN_MODULE_FFMPEG=ON 
    -DVRN_MODULE_FLOWREEN=ON
    -DVRN_MODULE_HDF5=ON
    -DVRN_USE_HDF5_VERSION=1.10
    -DVRN_MODULE_PLOTTING=ON
    -DVRN_MODULE_POI=ON
    -DVRN_MODULE_PVM=ON
    -DVRN_MODULE_RANDOMWALKER=ON
    -DVRN_MODULE_SEGY=ON
    -DVRN_MODULE_VOLUMELABELING=OFF 
    -DVRN_MODULE_DEVIL=ON
    -DVRN_MODULE_ZIP=ON
    -DVRN_MODULE_TIFF=ON
    #-DVRN_MODULE_TOUCHTABLE=ON
    -DVRN_MODULE_PYTHON=ON
    -DVRN_MODULE_OPENCL=OFF #Until we figure out how to run this in ci
    -DVRN_MODULE_OPENMP=ON
    -DVRN_MODULE_GDCM=ON
    #-DVRN_MODULE_ITK=ON
    -DVRN_MODULE_STAGING=ON
    -DVRN_MODULE_SURFACE=ON
    #-DVRN_MODULE_DEPRECATED=ON #Only compatibility
    -DVRN_MODULE_STEREOSCOPY=ON

    # custom modules
    -DVRN_MODULE_GRAPHLAYOUT=OFF

    # do not block on failed assertions
    -DVRN_NON_INTERACTIVE=ON

    -DVRN_PRECOMPILED_HEADER=OFF
)
#export CMAKE_PREFIX_PATH=/usr/local/HDF5-1.8.16-Linux/HDF_Group/HDF5/1.8.16/share/cmake/:$CMAKE_PREFIX_PATH
cmake "${build_options[@]}" ../voreen

# start build once again to make errors (if any) more readable

N_CORES=$(nproc)
nice make -j$N_CORES || make
