#!/bin/bash

#cd ${VOREEN_ROOT}

# clear build directory (also clears CMake cache)
BUILD_DIR=$1
rm -rf $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

# build configuration
build_options=(
    -DVRN_OPENGL_COMPATIBILITY_PROFILE=OFF #required to build some modules

    -DVRN_BUILD_VOREENVE=OFF
    -DVRN_BUILD_VOREENTOOL=ON
    -DVRN_BUILD_SIMPLEGLUT=OFF
    -DVRN_BUILD_SIMPLEQT=OFF
    -DVRN_BUILD_TESTAPPS=ON
    -DVRN_BUILD_BLASTEST=OFF
    -DVRN_BUILD_ITKWRAPPER=OFF
    #-DVRN_DEPLOYMENT=ON

    # public modules
    -DVRN_MODULE_BASE=ON
    -DVRN_MODULE_CONNEXE=OFF
    #-DVRN_MODULE_DYNAMICGLSL=ON #Only compatibility
    #-DVRN_MODULE_EXPERIMENTAL=ON #Only compatibility
    -DVRN_MODULE_FFMPEG=OFF
    -DVRN_MODULE_FLOWREEN=OFF
    -DVRN_MODULE_HDF5=OFF
    -DVRN_MODULE_PLOTTING=OFF
    -DVRN_MODULE_POI=OFF
    -DVRN_MODULE_PVM=OFF
    -DVRN_MODULE_RANDOMWALKER=OFF
    -DVRN_MODULE_SEGY=OFF
    -DVRN_MODULE_VOLUMELABELING=OFF 
    -DVRN_MODULE_DEVIL=OFF
    -DVRN_MODULE_ZIP=OFF
    -DVRN_MODULE_TIFF=OFF
    #-DVRN_MODULE_TOUCHTABLE=ON
    -DVRN_MODULE_PYTHON=OFF
    -DVRN_MODULE_OPENCL=OFF
    -DVRN_MODULE_OPENMP=OFF
    -DVRN_MODULE_GDCM=OFF
    #-DVRN_MODULE_ITK=ON
    -DVRN_MODULE_STAGING=OFF
    -DVRN_MODULE_SURFACE=OFF
    #-DVRN_MODULE_DEPRECATED=ON #Only compatibility
    -DVRN_MODULE_STEREOSCOPY=OFF

    # custom modules
    -DVRN_MODULE_GRAPHLAYOUT=OFF

    -DVRN_PRECOMPILED_HEADER=OFF
)
#export CMAKE_PREFIX_PATH=/usr/local/HDF5-1.8.16-Linux/HDF_Group/HDF5/1.8.16/share/cmake/:$CMAKE_PREFIX_PATH
cmake "${build_options[@]}" ../voreen

# start build once again to make errors (if any) more readable

N_CORES=$(nproc)
nice make -j$N_CORES || make