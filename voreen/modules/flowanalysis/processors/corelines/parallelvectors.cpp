/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2020 University of Muenster, Germany,                        *
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

#include "parallelvectors.h"

#include "voreen/core/ports/volumeport.h"
#include "voreen/core/ports/conditions/portconditionvolumetype.h"
#include "voreen/core/datastructures/volume/operators/volumeoperatorconvert.h"
#include "voreen/core/datastructures/geometry/pointlistgeometry.h"

#include "tgt/matrix.h"

#include "Eigen/Eigenvalues"

#include <vector>

namespace voreen {

ParallelVectors::ParallelVectors()
    : Processor()
    , _inV(Port::INPORT, "in_v", "Vector field V")
    , _inW(Port::INPORT, "in_w", "Vector field W")
    , _inJacobi(Port::INPORT, "in_jacobi", "Jacobi Matrix (Optional)")
    , _inMask(Port::INPORT, "inportMask", "Mask (Optional)")
    , _out(Port::OUTPORT, "outport", "Parallel Vector Solution Points")
    , _sujudiHaimes("sujudiHaimes", "Use Sujudi-Haimes method for filtering", false, Processor::VALID)
{
    this->addPort(_inV);
    _inV.addCondition(new PortConditionVolumeType3xFloat());
    this->addPort(_inW);
    _inW.addCondition(new PortConditionVolumeType3xFloat());
    this->addPort(_inJacobi);
    _inJacobi.addCondition(new PortConditionVolumeType("Matrix3(float)", "Volume_Mat3Float"));
    ON_CHANGE(_inJacobi, ParallelVectors, onChangedJacobianData);
    this->addPort(_inMask);
    this->addPort(_out);

    this->addProperty(_sujudiHaimes);
    _sujudiHaimes.setReadOnlyFlag(true);
}

void ParallelVectors::onChangedJacobianData() {
    _sujudiHaimes.setReadOnlyFlag(!_inJacobi.hasData());
}

bool ParallelVectors::isReady() const
{
    if(!_inV.isReady() || !_inW.isReady()) {
        setNotReadyErrorMessage("V and W must both be defined");
        return false;
    }

    const auto* volumeV = _inV.getData();
    const auto* volumeW = _inW.getData();
    const auto* volumeJacobi = _inJacobi.getData();

    if (volumeV->getDimensions() != volumeW->getDimensions() || (volumeJacobi && volumeV->getDimensions() != volumeJacobi->getDimensions()))
    {
        setNotReadyErrorMessage("Input dimensions do not match");
        return false;
    }

    const auto dim = volumeV->getDimensions();
    if (std::min({dim.x, dim.y, dim.z}) < 2)
    {
        setNotReadyErrorMessage("Input dimensions must be greater than 1 in each dimension");
        return false;
    }

    return true;
}

void ParallelVectors::setDescriptions() {
    setDescription("This processor implements the parallel vectors operator by Peikert and Roth and optional sujudi-haimes filtering. "
                   "It can be used to extract vortex corelines by using velocity, acceleration volumes, as well as the jacobi matrix as input.");
    _inV.setDescription("First input volume (V) for the parallel vectors operator");
    _inW.setDescription("Second input volume (W) for the parallel vectors operator");
    _inJacobi.setDescription("(Optional) Jacobi matrix to be used for sujudi-haimes filtering");
}

void ParallelVectors::Process( const VolumeRAM_3xFloat& V, const VolumeRAM_3xFloat& W, const VolumeRAM_Mat3Float* jacobi, const VolumeRAM* mask, ParallelVectorSolutions& outSolution )
{
    const auto dim = V.getDimensions();
    auto triangleSolutions = std::vector<tgt::vec3>();
    auto triangleSolutionIndices = std::vector<int32_t>((dim.x - 1) * (dim.y - 1) * (dim.z - 1) * TetrahedraPerCube * TrianglesPerTetrahedron, -1);

    const auto trianglesPerXInc = 24;
    const auto trianglesPerYInc = ( dim.x - 1 ) * TetrahedraPerCube * TrianglesPerTetrahedron;
    const auto trianglesPerZInc = ( dim.x - 1 ) * ( dim.y - 1 ) * TetrahedraPerCube * TrianglesPerTetrahedron;
    const std::array<int32_t, 24> partnerTriangleOffsets
    {
        -trianglesPerZInc + 19, 20, 4, trianglesPerYInc + 5,
        -trianglesPerZInc + 11, 4, -4, trianglesPerXInc + 13,
        -trianglesPerYInc - 5, -4, 4, trianglesPerXInc + 5,
        -trianglesPerYInc + 11, 4, -4, trianglesPerZInc - 11,
        -trianglesPerXInc - 5, -4, 4, trianglesPerZInc - 19,
        -trianglesPerXInc - 13, -20, -4, trianglesPerYInc - 11
    };

    auto voxels = std::vector<tgt::ivec3>();
    voxels.reserve( V.getNumVoxels() );
    for( long x = 0; x < dim.x - 1; ++x ) {
        for( long y = 0; y < dim.y - 1; ++y ) {
            for (long z = 0; z < dim.z - 1; ++z) {
                if (!mask || mask->getVoxelNormalized(x, y, z) > 0.0)
                    voxels.push_back(tgt::ivec3(x, y, z));
            }
        }
    }
    voxels.shrink_to_fit();


#pragma omp parallel for
    for( long voxelIndex = 0; voxelIndex < static_cast<long>( voxels.size() ); ++voxelIndex )
    {
        const auto x = static_cast<size_t>( voxels[voxelIndex].x );
        const auto y = static_cast<size_t>( voxels[voxelIndex].y );
        const auto z = static_cast<size_t>( voxels[voxelIndex].z );

        const std::array<const Tet, TetrahedraPerCube> cubeTets{
            Tet{																			  // front top left tet 0
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z}, {x + 1, y + 1, z}},			  // front 0
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z}, {x + 1, y + 1, z + 1}},		  // back 1
                Triangle{tgt::svec3{x, y, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}},	  // right 2
                Triangle{tgt::svec3{x, y + 1, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}}}, // top 3
                Tet{																			  // front bottom right tet 1
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z}, {x + 1, y + 1, z}},			  // front 4
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z}, {x + 1, y + 1, z + 1}},		  // back 5
                Triangle{tgt::svec3{x, y, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}},	  // left 6
                Triangle{tgt::svec3{x + 1, y, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}}}, // right 7
                Tet{																			  // middle right bottom tet 2
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z}, {x + 1, y, z + 1}},			  // bottom 8
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z}, {x + 1, y + 1, z + 1}},		  // front 9
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z + 1}, {x + 1, y + 1, z + 1}},	  // left 10
                Triangle{tgt::svec3{x + 1, y, z}, {x + 1, y, z + 1}, {x + 1, y + 1, z + 1}}}, // right 11
                Tet{																			  // back right bottom tet 3
                Triangle{tgt::svec3{x, y, z}, {x, y, z + 1}, {x + 1, y, z + 1}},			  // bottom 12
                Triangle{tgt::svec3{x, y, z}, {x, y, z + 1}, {x + 1, y + 1, z + 1}},		  // left 13
                Triangle{tgt::svec3{x, y, z}, {x + 1, y, z + 1}, {x + 1, y + 1, z + 1}},	  // right 14
                Triangle{tgt::svec3{x, y, z + 1}, {x + 1, y, z + 1}, {x + 1, y + 1, z + 1}}}, // back 15
                Tet{																			  // back left bottom tet 4
                Triangle{tgt::svec3{x, y, z}, {x, y, z + 1}, {x, y + 1, z + 1}},			  // left 16
                Triangle{tgt::svec3{x, y, z}, {x, y, z + 1}, {x + 1, y + 1, z + 1}},		  // right 17
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z + 1}, {x + 1, y + 1, z + 1}},	  // front 18
                Triangle{tgt::svec3{x, y, z + 1}, {x, y + 1, z + 1}, {x + 1, y + 1, z + 1}}}, // back 19
                Tet{																			  // middle left top tet 5
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z}, {x, y + 1, z + 1}},			  // left 20
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z}, {x + 1, y + 1, z + 1}},		  // front 21
                Triangle{tgt::svec3{x, y, z}, {x, y + 1, z + 1}, {x + 1, y + 1, z + 1}},	  // back 22
                Triangle{tgt::svec3{x, y + 1, z}, {x, y + 1, z + 1}, {x + 1, y + 1, z + 1} /* top 23 */}}};

        for (size_t tetIndexInCube = 0; tetIndexInCube < cubeTets.size(); ++tetIndexInCube)
        {
            for (size_t triIndexInTet = 0; triIndexInTet < TrianglesPerTetrahedron; ++triIndexInTet)
            {
                const auto triangleSolutionIndex = TrianglesPerTetrahedron * (TetrahedraPerCube * ((dim.x - 1) * ((dim.y - 1) * z + y) + x) + tetIndexInCube) + triIndexInTet;
                const auto partnerTriangleSolutionIndex = triangleSolutionIndex + partnerTriangleOffsets[tetIndexInCube * TrianglesPerTetrahedron + triIndexInTet];


                if(triangleSolutionIndices[triangleSolutionIndex] != -1) continue;
                if(partnerTriangleSolutionIndex >= 0 && partnerTriangleSolutionIndex < triangleSolutionIndices.size())
                    triangleSolutionIndices[partnerTriangleSolutionIndex] = -2;


                using to_eigen = Eigen::Map<const Eigen::Vector3f>;

                const auto triangle = cubeTets[tetIndexInCube][triIndexInTet];
                const auto vol1Voxel0 = to_eigen(V.voxel(triangle[0]).elem).cast<double>();
                const auto vol1Voxel1 = to_eigen(V.voxel(triangle[1]).elem).cast<double>();
                const auto vol1Voxel2 = to_eigen(V.voxel(triangle[2]).elem).cast<double>();

                const auto vol2Voxel0 = to_eigen(W.voxel(triangle[0]).elem).cast<double>();
                const auto vol2Voxel1 = to_eigen(W.voxel(triangle[1]).elem).cast<double>();
                const auto vol2Voxel2 = to_eigen(W.voxel(triangle[2]).elem).cast<double>();

                auto doBreak = false;
                for (auto i = 0; i < 3; ++i) // Check if vectors are too small at triangle vertices
                {
                    if (tgt::lengthSq(V.voxel(triangle[i])) < 0.00000000000001 || tgt::lengthSq(W.voxel(triangle[i])) < 0.00000000000001)
                    {
                        doBreak = true;
                        break;
                    }
                }
                if (doBreak)
                    break;

                const auto V = (Eigen::Matrix3d() << vol1Voxel0, vol1Voxel1, vol1Voxel2).finished();
                const auto W = (Eigen::Matrix3d() << vol2Voxel0, vol2Voxel1, vol2Voxel2).finished();
                Eigen::Matrix3d M, inverse;
                double det;
                bool invertible;

                V.computeInverseAndDetWithCheck(inverse, det, invertible, 0.00000001);
                if (invertible)
                    M = inverse * W;
                else
                {
                    W.computeInverseAndDetWithCheck(inverse, det, invertible, 0.00000001);
                    if (invertible)
                        M = inverse * V;
                    else
                    {
                        continue;
                    }
                }

                // V or W is invertible => find a solution now

                const auto eigensolver = Eigen::EigenSolver<Eigen::Matrix3d>(M);
                const auto &eigenvalues = eigensolver.eigenvalues();
                auto eigenvectors = eigensolver.eigenvectors();

                for (int eigenVectorIndex = 0; eigenVectorIndex < 3; ++eigenVectorIndex)
                {
                    const auto isReal = !eigenvalues(eigenVectorIndex).imag();
                    const auto sameSign = std::signbit(eigenvectors.col(eigenVectorIndex).x().real()) == std::signbit(eigenvectors.col(eigenVectorIndex).y().real()) && std::signbit(eigenvectors.col(eigenVectorIndex).y().real()) == std::signbit(eigenvectors.col(eigenVectorIndex).z().real());

                    if (!isReal || !sameSign)
                        continue;

                    eigenvectors.col(eigenVectorIndex) /= eigenvectors.col(eigenVectorIndex).sum();

                    const auto pos = static_cast<float>(eigenvectors.col(eigenVectorIndex).x().real()) * static_cast<const tgt::vec3 &>(cubeTets[tetIndexInCube][triIndexInTet][0]) + static_cast<float>(eigenvectors.col(eigenVectorIndex).y().real()) * static_cast<const tgt::vec3 &>(cubeTets[tetIndexInCube][triIndexInTet][1]) + static_cast<float>(eigenvectors.col(eigenVectorIndex).z().real()) * static_cast<const tgt::vec3 &>(cubeTets[tetIndexInCube][triIndexInTet][2]);

                    auto addSolution = !jacobi;

                    if (jacobi)
                    {
                        // Interpolate jacobian at solution (barycentric coordinates)
                        const auto jacobianAtSolution = static_cast<float>(eigenvectors.col(eigenVectorIndex).x().real()) * jacobi->voxel(cubeTets[tetIndexInCube][triIndexInTet][0]) + static_cast<float>(eigenvectors.col(eigenVectorIndex).y().real()) * jacobi->voxel(cubeTets[tetIndexInCube][triIndexInTet][1]) + static_cast<float>(eigenvectors.col(eigenVectorIndex).z().real()) * jacobi->voxel(cubeTets[tetIndexInCube][triIndexInTet][2]);

                        using EigenMat3fRowMajor = Eigen::Matrix<float, 3, 3, Eigen::StorageOptions::RowMajor | Eigen::StorageOptions::AutoAlign>;
                        const auto jacobianEigenvalues = Eigen::EigenSolver<EigenMat3fRowMajor>(reinterpret_cast<const EigenMat3fRowMajor &>(jacobianAtSolution), false).eigenvalues();

                        int numberOfComplexEigenvalues = 0;
                        for (int j = 0; j < 3; ++j) // count complex eigenvalues of jacobian
                        {
                            if (jacobianEigenvalues(j).imag())
                            {
                                ++numberOfComplexEigenvalues;
                            }
                        }
                        addSolution = numberOfComplexEigenvalues == 2;
                    }

                    if (addSolution)
                    {
#pragma omp critical
                        {
                            triangleSolutionIndices[triangleSolutionIndex] = triangleSolutions.size();
                            if(partnerTriangleSolutionIndex >= 0 && partnerTriangleSolutionIndex < triangleSolutionIndices.size())
                                triangleSolutionIndices[partnerTriangleSolutionIndex] = triangleSolutions.size();
                            triangleSolutions.push_back(pos);
                        }
                        break;
                    }
                }
            }
        }
    }

    outSolution.dimensions = dim;
    outSolution.solutions.swap(triangleSolutions);
    outSolution.triangleSolutionIndices.swap(triangleSolutionIndices);
}

void ParallelVectors::process() {

    VolumeRAMRepresentationLock volumeV(_inV.getData());
    VolumeRAMRepresentationLock volumeW(_inW.getData());

    std::unique_ptr<VolumeRAMRepresentationLock> volumeJacobi;
    if(_sujudiHaimes.get() && _inJacobi.hasData()) {
        volumeJacobi.reset(new VolumeRAMRepresentationLock(_inJacobi.getData()));
    }
    const auto* jacobi = volumeJacobi ? dynamic_cast<const VolumeRAM_Mat3Float*>(**volumeJacobi) : nullptr;

    std::unique_ptr<VolumeRAMRepresentationLock> volumeMask;
    if(_inMask.hasData()) {
        volumeMask.reset(new VolumeRAMRepresentationLock(_inMask.getData()));
    }
    const auto* mask = volumeMask ? **volumeMask : nullptr;

    auto solutions = std::unique_ptr<ParallelVectorSolutions>( new ParallelVectorSolutions() );
    ParallelVectors::Process( *dynamic_cast<const VolumeRAM_3xFloat*>(*volumeV),
                              *dynamic_cast<const VolumeRAM_3xFloat*>(*volumeV),
                              jacobi, mask,*solutions );
    _out.setData(solutions.release());
}

} // namespace voreen