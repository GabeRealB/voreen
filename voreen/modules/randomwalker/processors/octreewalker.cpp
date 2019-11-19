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

#include "octreewalker.h"

#include "../solver/randomwalkerseeds.h"
#include "../solver/randomwalkerweights.h"

#include "voreen/core/datastructures/volume/volumeram.h"
#include "voreen/core/datastructures/volume/volume.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"
#include "voreen/core/datastructures/volume/volumeminmax.h"
#include "voreen/core/datastructures/volume/operators/volumeoperatorconvert.h"
#include "voreen/core/datastructures/volume/operators/volumeoperatormorphology.h"
#include "voreen/core/datastructures/volume/operators/volumeoperatorresample.h"
#include "voreen/core/datastructures/volume/operators/volumeoperatornumsignificant.h"
#include "voreen/core/datastructures/octree/octreebrickpoolmanagerdisk.h"
#include "voreen/core/datastructures/octree/volumeoctreenodegeneric.h"
#include "voreen/core/datastructures/geometry/pointsegmentlistgeometry.h"
#include "tgt/vector.h"
#include "tgt/memory.h"

#include "voreen/core/datastructures/transfunc/1d/transfunc1d.h"

#include <climits>

#ifdef VRN_MODULE_OPENMP
#include <omp.h>
#endif

namespace voreen {

#if defined(VRN_MODULE_OPENMP) && 1
#define VRN_OCTREEWALKER_USE_OMP
#endif

//#define VRN_OCTREEWALKER_MEAN_NOT_MEDIAN

const std::string OctreeWalker::loggerCat_("voreen.RandomWalker.OctreeWalker");
using tgt::vec3;

OctreeWalker::OctreeWalker()
    : AsyncComputeProcessor<OctreeWalkerInput, OctreeWalkerOutput>(),
    inportVolume_(Port::INPORT, "volume.input"),
    inportForegroundSeeds_(Port::INPORT, "geometry.seedsForeground", "geometry.seedsForeground", true),
    inportBackgroundSeeds_(Port::INPORT, "geometry.seedsBackground", "geometry.seedsBackground", true),
    outportProbabilities_(Port::OUTPORT, "volume.probabilities", "volume.probabilities", false),
    usePrevProbAsInitialization_("usePrevProbAsInitialization", "Use Previous Probabilities as Initialization", false, Processor::VALID, Property::LOD_ADVANCED),
    minEdgeWeight_("minEdgeWeight", "Min Edge Weight: 10^(-t)", 5, 0, 10),
    preconditioner_("preconditioner", "Preconditioner"),
    errorThreshold_("errorThreshold", "Error Threshold: 10^(-t)", 2, 0, 10),
    maxIterations_("conjGradIterations", "Max Iterations", 1000, 1, 5000),
    conjGradImplementation_("conjGradImplementation", "Implementation"),
    homogeneityThreshold_("homogeneityThreshold", "Homogeneity Threshold", 0.01, 0.0, 1.0),
    currentInputVolume_(0)
{
    // ports
    addPort(inportVolume_);
    addPort(inportForegroundSeeds_);
    addPort(inportBackgroundSeeds_);
    addPort(outportProbabilities_);

    addProperty(usePrevProbAsInitialization_);

    // random walker properties
    addProperty(minEdgeWeight_);
    minEdgeWeight_.setGroupID("rwparam");
    setPropertyGroupGuiName("rwparam", "Random Walker Parametrization");
    addProperty(homogeneityThreshold_);
    homogeneityThreshold_.setGroupID("rwparam");
    homogeneityThreshold_.adaptDecimalsToRange(5);

    // conjugate gradient solver
    preconditioner_.addOption("none", "None");
    preconditioner_.addOption("jacobi", "Jacobi");
    preconditioner_.select("jacobi");
    addProperty(preconditioner_);
    addProperty(errorThreshold_);
    addProperty(maxIterations_);
    conjGradImplementation_.addOption("blasCPU", "CPU");
#ifdef VRN_MODULE_OPENMP
    conjGradImplementation_.addOption("blasMP", "OpenMP");
    conjGradImplementation_.select("blasMP");
#endif
#ifdef VRN_MODULE_OPENCL
    conjGradImplementation_.addOption("blasCL", "OpenCL");
    conjGradImplementation_.select("blasCL");
#endif
    addProperty(conjGradImplementation_);
    preconditioner_.setGroupID("conjGrad");
    errorThreshold_.setGroupID("conjGrad");
    maxIterations_.setGroupID("conjGrad");
    conjGradImplementation_.setGroupID("conjGrad");
    setPropertyGroupGuiName("conjGrad", "Conjugate Gradient Solver");
}

OctreeWalker::~OctreeWalker() {
}

Processor* OctreeWalker::create() const {
    return new OctreeWalker();
}

void OctreeWalker::initialize() {
    AsyncComputeProcessor::initialize();

#ifdef VRN_MODULE_OPENCL
    voreenBlasCL_.initialize();
#endif

    updateGuiState();
}

void OctreeWalker::deinitialize() {
    AsyncComputeProcessor::deinitialize();
}

bool OctreeWalker::isReady() const {
    bool ready = false;
    ready |= outportProbabilities_.isConnected();
    ready &= inportVolume_.isReady();
    ready &= inportForegroundSeeds_.isReady();
    ready &= inportBackgroundSeeds_.isReady();
    return ready;
}

OctreeWalker::ComputeInput OctreeWalker::prepareComputeInput() {
    //edgeWeightTransFunc_.setVolume(inportVolume_.getData());

    tgtAssert(inportVolume_.hasData(), "no input volume");

    // clear previous results and update property ranges, if input volume has changed
    if (inportVolume_.hasChanged()) {
        outportProbabilities_.setData(0);
    }
    auto vol = inportVolume_.getThreadSafeData();

    // clear previous results and update property ranges, if input volume has changed
    if (!vol->hasRepresentation<VolumeOctree>()) {
        throw InvalidInputException("No octree Representation", InvalidInputException::S_ERROR);
    }
    const VolumeOctree* octreePtr = vol->getRepresentation<VolumeOctree>();
    tgtAssert(octreePtr, "No octree");


    // select BLAS implementation and preconditioner
    const VoreenBlas* voreenBlas = getVoreenBlasFromProperties();
    VoreenBlas::ConjGradPreconditioner precond = VoreenBlas::NoPreconditioner;
    if (preconditioner_.isSelected("jacobi"))
        precond = VoreenBlas::Jacobi;


    std::vector<float> prevProbs;

    float errorThresh = 1.f / pow(10.f, static_cast<float>(errorThreshold_.get()));
    int maxIterations = maxIterations_.get();

    return ComputeInput {
        *vol,
        *octreePtr,
        inportForegroundSeeds_.getThreadSafeAllData(),
        inportBackgroundSeeds_.getThreadSafeAllData(),
        minEdgeWeight_.get(),
        voreenBlas,
        precond,
        errorThresh,
        maxIterations,
        homogeneityThreshold_.get(),
    };
}

static uint16_t normToBrick(float val) {
    return tgt::clamp(val, 0.0f, 1.0f) * 0xffff;
}
static float brickToNorm(uint16_t val) {
    return static_cast<float>(val)/0xffff;
}

static void getSeedListsFromPorts(std::vector<PortDataPointer<Geometry>>& geom, PointSegmentListGeometry<tgt::vec3>& seeds) {

    for (size_t i=0; i<geom.size(); i++) {
        const PointSegmentListGeometry<tgt::vec3>* seedList = dynamic_cast<const PointSegmentListGeometry<tgt::vec3>* >(geom.at(i).get());
        if (!seedList)
            LWARNINGC("voreen.RandomWalker.OctreeWalker", "Invalid geometry. PointSegmentListGeometry<vec3> expected.");
        else {
            auto transformMat = seedList->getTransformationMatrix();
            for (int j=0; j<seedList->getNumSegments(); j++) {
                std::vector<tgt::vec3> points;
                for(auto& vox : seedList->getSegment(j)) {
                    points.push_back(transformMat.transform(vox));
                }
                seeds.addSegment(points);
            }
        }
    }
}

static const VolumeOctreeNode& findLeafNodeFor(const VolumeOctreeNode& root, tgt::svec3& llf, tgt::svec3& urb, size_t& level, const tgt::svec3& point, const tgt::svec3& brickDataSize, size_t targetLevel) {
    tgtAssert(tgt::hand(tgt::lessThanEqual(llf, point)) && tgt::hand(tgt::lessThan(point, urb)), "Invalid point pos");

    if(root.isLeaf() || level == targetLevel) {
        return root;
    }

    tgtAssert(level >= 0, "Invalid level");
    tgt::svec3 newLlf = llf;
    tgt::svec3 newUrb = urb;
    size_t newLevel = level - 1;
    tgt::svec3 brickSize = brickDataSize * (1UL << newLevel);
    size_t index = 0;
    if(point.x >= llf.x+brickSize.x) {
        index += 1;
        newLlf.x = llf.x+brickSize.x;
    } else {
        newUrb.x = llf.x+brickSize.x;
    }
    if(point.y >= llf.y+brickSize.y) {
        index += 2;
        newLlf.y = llf.y+brickSize.y;
    } else {
        newUrb.y = llf.y+brickSize.y;
    }
    if(point.z >= llf.z+brickSize.z) {
        index += 4;
        newLlf.z = llf.z+brickSize.z;
    } else {
        newUrb.z = llf.z+brickSize.z;
    }

    const VolumeOctreeNode* child = root.children_[index];
    tgtAssert(child, "No child in non leaf node");

    if(child->isHomogeneous()) {
        // Parent has better resolution
        return root;
    }
    level = newLevel;
    urb = newUrb;
    llf = newLlf;
    return findLeafNodeFor(*child, llf, urb, level, point, brickDataSize, targetLevel);
}
struct OctreeWalkerNode {
    OctreeWalkerNode(const VolumeOctreeNode& node, size_t level, tgt::svec3 llf, tgt::svec3 urb)
        : node_(node)
        , level_(level)
        , llf_(llf)
        , urb_(urb)
    {
    }
    OctreeWalkerNode findChildNode(const tgt::svec3& point, const tgt::svec3& brickDataSize, size_t targetLevel) const {
        tgtAssert(level_ >= targetLevel, "Invalid target level");
        size_t level = level_;
        tgt::svec3 llf = llf_;
        tgt::svec3 urb = urb_;

        const VolumeOctreeNode& node = findLeafNodeFor(node_, llf, urb, level, point, brickDataSize, targetLevel);
        return OctreeWalkerNode(node, level, llf, urb);
    }

    tgt::svec3 voxelDimensions() const {
        return urb_ - llf_;
    }

    tgt::svec3 brickDimensions() const {
        return voxelDimensions() / scale();
    }

    size_t scale() const {
        return 1 << level_;
    }

    tgt::mat4 voxelToBrick() const {
        return tgt::mat4::createScale(tgt::vec3(1.0f/scale())) * tgt::mat4::createTranslation(-tgt::vec3(llf_));
    }

    tgt::mat4 brickToVoxel() const {
        return tgt::mat4::createTranslation(llf_) * tgt::mat4::createScale(tgt::vec3(scale()));
    }

    const VolumeOctreeNode& node_;
    size_t level_;
    tgt::svec3 llf_;
    tgt::svec3 urb_;

};

// TODO Move OctreeBrickPoolManager and generate from member function => Const and mutable variant
struct OctreeWalkerNodeBrick {
    OctreeWalkerNodeBrick() = delete;
    OctreeWalkerNodeBrick(const OctreeWalkerNodeBrick&) = delete;
    OctreeWalkerNodeBrick& operator=(const OctreeWalkerNodeBrick&) = delete;

    OctreeWalkerNodeBrick(uint64_t addr, const tgt::svec3& brickDataSize, const OctreeBrickPoolManagerBase& pool)
        : addr_(addr)
        , data_(pool.getWritableBrick(addr_), brickDataSize, false) // data is not owned!
        , pool_(pool)
    {
    }
    ~OctreeWalkerNodeBrick() {
        pool_.releaseBrick(addr_, OctreeBrickPoolManagerBase::WRITE);
    }
    float getVoxelNormalized(const tgt::svec3& pos) const {
        return brickToNorm(data_.voxel(pos));
    }
    uint64_t addr_;
    VolumeAtomic<uint16_t> data_;
    const OctreeBrickPoolManagerBase& pool_;
};
struct OctreeWalkerNodeBrickConst {
    OctreeWalkerNodeBrickConst() = delete;
    OctreeWalkerNodeBrickConst(const OctreeWalkerNodeBrickConst&) = delete;
    OctreeWalkerNodeBrickConst& operator=(const OctreeWalkerNodeBrickConst&) = delete;

    OctreeWalkerNodeBrickConst(uint64_t addr, const tgt::svec3& brickDataSize, const OctreeBrickPoolManagerBase& pool)
        : addr_(addr)
        , data_(const_cast<uint16_t*>(pool.getBrick(addr_)), brickDataSize, false) // data is not owned!
        , pool_(pool)
    {
    }
    ~OctreeWalkerNodeBrickConst() {
        pool_.releaseBrick(addr_, OctreeBrickPoolManagerBase::READ);
    }

    float getVoxelNormalized(const tgt::svec3& pos) const {
        return brickToNorm(data_.voxel(pos));
    }
    uint64_t addr_;
    const VolumeAtomic<uint16_t> data_;
    const OctreeBrickPoolManagerBase& pool_;
};

namespace {

inline size_t volumeCoordsToIndex(int x, int y, int z, const tgt::ivec3& dim) {
    return z*dim.y*dim.x + y*dim.x + x;
}

inline size_t volumeCoordsToIndex(const tgt::ivec3& coords, const tgt::ivec3& dim) {
    return coords.z*dim.y*dim.x + coords.y*dim.x + coords.x;
}
}

struct BrickNeighborhood {
    BrickNeighborhood() = delete;
    BrickNeighborhood(const BrickNeighborhood& other) = delete;
    BrickNeighborhood& operator=(const BrickNeighborhood& other) = delete;
    BrickNeighborhood(BrickNeighborhood&& other) = default;
    BrickNeighborhood& operator=(BrickNeighborhood&& other) = default;

    tgt::mat4 centerBrickToNeighborhood() const {
        return tgt::mat4::createTranslation(tgt::vec3(centerBrickLlf_));
    }
    tgt::mat4 neighborhoodToCenterBrick() const {
        return tgt::mat4::createTranslation(-tgt::vec3(centerBrickLlf_));
    }
    tgt::mat4 voxelToNeighborhood() const {
        return centerBrickToNeighborhood() * voxelToCenterBrick_;
    }
    bool isEmpty() const {
        return data_.getDimensions() == tgt::svec3(0);
    }

    VolumeAtomic<float> data_;
    tgt::svec3 centerBrickLlf_; // In coordinate system ...
    tgt::svec3 centerBrickUrb_; // ... of seed buffer
    tgt::svec3 dimensions_;
    tgt::mat4 voxelToCenterBrick_;
    float min_;
    float max_;
    float avg_;

    static BrickNeighborhood empty(tgt::svec3 dimensions, int scale) {
        return BrickNeighborhood {
            VolumeAtomic<float>(tgt::svec3(0)),
            tgt::svec3(0),
            dimensions,
            dimensions,
            tgt::mat4::identity,
            0.0f,
            0.0f,
            0.0f,
        };
    }
    static BrickNeighborhood fromNode(const OctreeWalkerNode& current, size_t sampleLevel, const OctreeWalkerNode& root, const tgt::svec3& brickBaseSize, const OctreeBrickPoolManagerBase& brickPoolManager) {
        const tgt::svec3 volumeDim = root.voxelDimensions();

        const tgt::mat4 brickToVoxel = current.brickToVoxel();
        const tgt::mat4 voxelToBrick = current.voxelToBrick();

        //const tgt::ivec3 neighborhoodSize = brickBaseSize;
        //const tgt::ivec3 neighborhoodSize = tgt::ivec3(2);
        const tgt::ivec3 neighborhoodSize = brickBaseSize/8UL;

        const tgt::ivec3 brickLlf(0);
        const tgt::ivec3 brickUrb = voxelToBrick.transform(current.urb_);

        const tgt::svec3 voxelLlf = tgt::max(tgt::vec3(0),         brickToVoxel.transform(brickLlf - neighborhoodSize));
        const tgt::svec3 voxelUrb = tgt::min(tgt::vec3(volumeDim), brickToVoxel.transform(brickUrb + neighborhoodSize));

        const tgt::ivec3 regionLlf = voxelToBrick.transform(voxelLlf);
        const tgt::ivec3 regionUrb = voxelToBrick.transform(voxelUrb);

        const tgt::svec3 regionDim = regionUrb - regionLlf;

        VolumeAtomic<float> output(regionDim);

        float min = std::numeric_limits<float>::infinity();
        float max = -std::numeric_limits<float>::infinity();
        float sum = 0.0f;

        VRN_FOR_EACH_VOXEL(blockIndex, tgt::svec3(0), tgt::svec3(3)) {
            tgt::ivec3 blockLlf;
            tgt::ivec3 blockUrb;
            for(int dim=0; dim<3; ++dim) {
                switch(blockIndex[dim]) {
                    case 0: {
                        blockLlf[dim] = regionLlf[dim];
                        blockUrb[dim] = brickLlf[dim];
                        break;
                    }
                    case 1: {
                        blockLlf[dim] = brickLlf[dim];
                        blockUrb[dim] = brickUrb[dim];
                        break;
                    }
                    case 2: {
                        blockLlf[dim] = brickUrb[dim];
                        blockUrb[dim] = regionUrb[dim];
                        break;
                    }
                }
            }
            tgt::svec3 blockDimensions = blockUrb - blockLlf;
            if(tgt::hor(tgt::equal(blockDimensions, tgt::svec3(0)))) {
                continue;
            }
            tgt::svec3 samplePoint = brickToVoxel.transform(blockLlf);
            auto node = root.findChildNode(samplePoint, brickBaseSize, sampleLevel);
            if(node.node_.hasBrick()) {
                OctreeWalkerNodeBrickConst brick(node.node_.getBrickAddress(), brickBaseSize, brickPoolManager);

                float sum = 0.0f;
                tgt::mat4 centerToSampleBrick = node.voxelToBrick() * brickToVoxel;
                VRN_FOR_EACH_VOXEL(point, blockLlf, blockUrb) {
                    tgt::vec3 samplePos = centerToSampleBrick.transform(point);
                    samplePos = tgt::clamp(samplePos, tgt::vec3(0), tgt::vec3(node.brickDimensions() - tgt::svec3(1)));
                    float val = brick.getVoxelNormalized(samplePos);
                    tgt::vec3 neighborhoodBufferPos = point - regionLlf;
                    output.setVoxelNormalized(val, neighborhoodBufferPos);
                    min = std::min(val, min);
                    max = std::max(val, max);
                    sum += val;
                }
            } else {
                float val = static_cast<float>(node.node_.getAvgValue())/0xffff;
                min = std::min(val, min);
                max = std::max(val, max);
                sum += val * tgt::hmul(blockUrb - blockLlf);
                VRN_FOR_EACH_VOXEL(point, blockLlf, blockUrb) {
                    tgt::vec3 neighborhoodBufferPos = point - regionLlf;
                    output.setVoxelNormalized(val, neighborhoodBufferPos);
                }
            }
        }
        float avg = sum / output.getNumVoxels();
        return BrickNeighborhood {
            std::move(output),
            -regionLlf,
            -regionLlf+brickUrb,
            regionDim,
            voxelToBrick,
            min,
            max,
            avg
        };
    }
};

class RandomWalkerSeedsBrick : public RandomWalkerSeeds {
    static const float UNLABELED;
    static const float FOREGROUND;
    static const float BACKGROUND;
public:
    RandomWalkerSeedsBrick(tgt::svec3 bufferDimensions, tgt::mat4 voxelToSeeds, const PointSegmentListGeometryVec3& foregroundSeedList, const PointSegmentListGeometryVec3& backgroundSeedList)
        : seedBuffer_(bufferDimensions)
    {
        VolumeAtomic<uint16_t> seedCounts(bufferDimensions);
        numSeeds_ = 0;
        seedBuffer_.fill(UNLABELED);
        seedCounts.fill(0);

        // foreground geometry seeds
        auto collectLabelsFromGeometry = [&] (const PointSegmentListGeometryVec3& seedList, uint8_t label) {
            for (int m=0; m<seedList.getNumSegments(); m++) {
                const std::vector<tgt::vec3>& foregroundPoints = seedList.getData()[m];
                if (foregroundPoints.empty())
                    continue;
                for (size_t i=0; i<foregroundPoints.size()-1; i++) {
                    tgt::vec3 left = voxelToSeeds*foregroundPoints[i];
                    tgt::vec3 right = voxelToSeeds*foregroundPoints[i+1];
                    tgt::vec3 dir = tgt::normalize(right - left);
                    for (float t=0.f; t<tgt::length(right-left); t += 1.f) {
                        tgt::ivec3 point = tgt::iround(left + t*dir);
                        if(tgt::hor(tgt::lessThan(point, tgt::ivec3::zero)) || tgt::hor(tgt::greaterThanEqual(point, tgt::ivec3(bufferDimensions)))) {
                            continue;
                        }
                        float& seedVal = seedBuffer_.voxel(point);
                        if (seedVal == UNLABELED) {
                            tgtAssert(seedCounts.voxel(point) == 0, "Invalid seed count");
                            seedVal = label;
                            ++numSeeds_;
                            seedCounts.voxel(point) = 1;
                        } else {
                            // On multiple points per label: Use average
                            tgtAssert(seedCounts.voxel(point) > 0, "Invalid seed count");
                            auto& count = seedCounts.voxel(point);
                            seedVal = ((seedVal*count)+label)/(count + 1);
                            count++;
                        }
                    }
                }
            }
        };
        collectLabelsFromGeometry(foregroundSeedList, FOREGROUND);
        collectLabelsFromGeometry(backgroundSeedList, BACKGROUND);
    }
    RandomWalkerSeedsBrick(RandomWalkerSeedsBrick&& other) = default;

    void addNeighborhoodBorderSeeds(const BrickNeighborhood& neighborhood) {
        tgtAssert(neighborhood.data_.getDimensions() == neighborhood.dimensions_, "Invalid buffer dimensions");

        auto collectLabelsFromNeighbor = [&] (size_t dim, size_t sliceIndex) {
            tgt::svec3 begin(0);
            tgt::svec3 end(neighborhood.dimensions_);

            begin[dim] = sliceIndex;
            end[dim] = sliceIndex+1;

            VRN_FOR_EACH_VOXEL(seed, begin, end) {
                float val = neighborhood.data_.voxel(seed);
                float& seedVal = seedBuffer_.voxel(seed);
                if (seedVal == UNLABELED) {
                    seedVal = val;
                    ++numSeeds_;
                }
            }
        };

        collectLabelsFromNeighbor(0, 0);
        collectLabelsFromNeighbor(0, neighborhood.dimensions_.x-1);
        collectLabelsFromNeighbor(1, 0);
        collectLabelsFromNeighbor(1, neighborhood.dimensions_.y-1);
        collectLabelsFromNeighbor(2, 0);
        collectLabelsFromNeighbor(2, neighborhood.dimensions_.z-1);
    }
    virtual ~RandomWalkerSeedsBrick() {}
    virtual void initialize() {};

    virtual bool isSeedPoint(size_t index) const {
        return seedBuffer_.voxel(index) != UNLABELED;
    }
    virtual bool isSeedPoint(const tgt::ivec3& voxel) const {
        return seedBuffer_.voxel(voxel) != UNLABELED;
    }
    virtual float getSeedValue(size_t index) const {
        return seedBuffer_.voxel(index);
    }
    virtual float getSeedValue(const tgt::ivec3& voxel) const {
        return seedBuffer_.voxel(voxel);
    }

    std::vector<size_t> generateVolumeToRowsTable() {
        size_t numVoxels = tgt::hmul(seedBuffer_.getDimensions());
        std::vector<size_t> volIndexToRow(numVoxels, -1);

        // compute volIndexToRow values
        size_t curRow = 0;
        for (size_t i=0; i<numVoxels; i++) {
            if (isSeedPoint(i)) {
                volIndexToRow[i] = -1;
            } else {
                volIndexToRow[i] = curRow;
                curRow++;
            }
        }
        return volIndexToRow;
    }

    tgt::svec3 bufferDimensions() const {
        return seedBuffer_.getDimensions();
    }

private:
    VolumeAtomic<float> seedBuffer_;
};
const float RandomWalkerSeedsBrick::UNLABELED = -1.0f;
const float RandomWalkerSeedsBrick::FOREGROUND = 1.0f;
const float RandomWalkerSeedsBrick::BACKGROUND = 0.0f;

struct RandomWalkerVoxelAccessorBrick final : public RandomWalkerVoxelAccessor {
    RandomWalkerVoxelAccessorBrick(const VolumeAtomic<float>& brick)
        : brick_(brick)
    {
    }
    inline float voxel(const tgt::svec3& pos) {
        return brick_.voxel(pos);
    }
private:
    const VolumeAtomic<float>& brick_;
};

static VolumeAtomic<float> preprocessImageForRandomWalker(const VolumeAtomic<float>& img) {
    VolumeAtomic<float> output(img.getDimensions());
    const tgt::ivec3 start(0);
    const tgt::ivec3 end(img.getDimensions());
    const size_t numVoxels = tgt::hmul(img.getDimensions());

    const int k = 1;
    const int N=2*k+1;
    const tgt::ivec3 neighborhoodSize(k);

    float sumOfDifferences = 0.0f;
    VRN_FOR_EACH_VOXEL(center, start, end) {
        const tgt::ivec3 neighborhoodStart = tgt::max(start, center - neighborhoodSize);
        const tgt::ivec3 neighborhoodEnd = tgt::min(end, center + neighborhoodSize + tgt::ivec3(1));

        const int numNeighborhoodVoxels = tgt::hmul(neighborhoodEnd-neighborhoodStart);

#ifdef VRN_OCTREEWALKER_MEAN_NOT_MEDIAN
        // mean
        float sum=0.0f;
        VRN_FOR_EACH_VOXEL(pos, neighborhoodStart, neighborhoodEnd) {
            sum += img.voxel(pos);
        }
        float estimation = sum/numNeighborhoodVoxels;
#else
        // median
        std::array<float, N*N*N> vals;
        int i=0;
        VRN_FOR_EACH_VOXEL(pos, neighborhoodStart, neighborhoodEnd) {
            vals[i++] = img.voxel(pos);
        }
        int centerIndex = i/2;
        std::nth_element(vals.begin(), vals.begin()+centerIndex, vals.begin()+i);
        //std::sort(vals.begin(), vals.begin()+i);
        float estimation = vals[centerIndex];
#endif

        float val = img.voxel(center);
        float diff = estimation - val;

        float neighborhoodFactor;
        if(numNeighborhoodVoxels > 1) {
            neighborhoodFactor = static_cast<float>(numNeighborhoodVoxels)/static_cast<float>(numNeighborhoodVoxels-1);
        } else {
            neighborhoodFactor = 1.0f;
        }

        sumOfDifferences += neighborhoodFactor * diff * diff;

        output.voxel(center) = estimation;
    }

#ifdef VRN_OCTREEWALKER_MEAN_NOT_MEDIAN
    const float varianceFactor = 2.0f/(N*N*N*N); //mean
#else
    tgtAssert(k==1, "Invalid k for variance factor");
    const float varianceFactor = 0.142; //median //TODO: this is for 2D. what about 3d?
#endif

    float rawVariance = sumOfDifferences/numVoxels;
    float varianceEstimation = rawVariance * varianceFactor;
    float stdEstimationInv = 1.0f/std::sqrt(varianceEstimation);

    VRN_FOR_EACH_VOXEL(center, start, end) {
        output.voxel(center) *= stdEstimationInv;
    }

    return output;
}
template<typename Accessor>
static void processVoxelWeights(const tgt::ivec3& voxel, const RandomWalkerSeeds* seeds, EllpackMatrix<float>& mat, float* vec, size_t* volumeIndexToRowTable, Accessor& voxelFun, const tgt::svec3& volDim, float minWeight) {
    auto edgeWeight = [minWeight] (float voxelIntensity, float neighborIntensity) {
        float beta = 0.5f;
        float intDiff = (voxelIntensity - neighborIntensity);
        float intDiffSqr = intDiff*intDiff;
        float weight = exp(-beta * intDiffSqr);
        weight = std::max(weight, minWeight);

        return weight;
    };
    tgtAssert(seeds, "no seed definer passed");
    tgtAssert(volumeIndexToRowTable, "no volumeIndexToRowTable passed");
    tgtAssert(mat.isInitialized(), "matrix not initialized");

    const int x = voxel.x;
    const int y = voxel.y;
    const int z = voxel.z;

    size_t index = volumeCoordsToIndex(voxel, volDim);
    if (seeds->isSeedPoint(index))
        return;

    size_t curRow = volumeIndexToRowTable[index];

    float curIntensity = voxelFun.voxel(voxel);

    float weightSum = 0;

    // x-neighbors
    if (x > 0) {
        tgt::ivec3 neighbor(x-1, y, z);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }
    if (x < volDim.x-1) {
        tgt::ivec3 neighbor(x+1, y, z);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }

    // y-neighbors
    if (y > 0) {
        tgt::ivec3 neighbor(x, y-1, z);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }
    if (y < volDim.y-1) {
        tgt::ivec3 neighbor(x, y+1, z);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }

    // z-neighbors
    if (z > 0) {
        tgt::ivec3 neighbor(x, y, z-1);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }
    if (z < volDim.z-1) {
        tgt::ivec3 neighbor(x, y, z+1);

        size_t neighborIndex = volumeCoordsToIndex(neighbor, volDim);
        float neighborIntensity = voxelFun.voxel(neighbor);
        float weight = edgeWeight(curIntensity, neighborIntensity);

        if (!seeds->isSeedPoint(neighbor)) {
            size_t nRow = volumeIndexToRowTable[neighborIndex];
            //tgtAssert(nRow >= 0 && nRow < numUnseeded_, "Invalid row");
            mat.setValue(curRow, nRow, -weight);
        }
        else {
            vec[curRow] += weight * seeds->getSeedValue(neighbor);
        }

        weightSum += weight;
    }

    mat.setValue(curRow, curRow, weightSum);
}
static uint64_t processOctreeBrick(OctreeWalkerInput& input, OctreeWalkerNode& outputNode, Histogram1D& histogram, uint16_t& min, uint16_t& max, uint16_t& avg, OctreeBrickPoolManagerBase& outputPoolManager, OctreeWalkerNode* outputRoot, const OctreeWalkerNode inputRoot, PointSegmentListGeometryVec3& foregroundSeeds, PointSegmentListGeometryVec3& backgroundSeeds, std::mutex& clMutex) {
    auto canSkipChildren = [&] (float min, float max) {
        float parentValueRange = max-min;
        const float delta = 0.01;
        bool minMaxSkip = max < 0.5-delta || min > 0.5+delta;
        return parentValueRange < input.homogeneityThreshold_ || minMaxSkip;
    };

    //TODO: catch out of memory
    const OctreeBrickPoolManagerBase& inputPoolManager = *input.octree_.getBrickPoolManager();
    const tgt::svec3 brickDataSize = input.octree_.getBrickDim();

    boost::optional<BrickNeighborhood> seedsNeighborhood = boost::none;

    bool stop = false;
    RandomWalkerSeedsBrick seeds = [&] () {
        if(outputRoot) {
            seedsNeighborhood = BrickNeighborhood::fromNode(outputNode, outputNode.level_+1, *outputRoot, brickDataSize, outputPoolManager);
            tgt::svec3 seedBufferDimensions = seedsNeighborhood->data_.getDimensions();
            tgt::mat4 voxelToSeedTransform = seedsNeighborhood->voxelToNeighborhood();

            if(canSkipChildren(seedsNeighborhood->min_, seedsNeighborhood->max_)) {
                //LINFOC("Randomwalker", "skip block early");
                stop = true;
                avg = normToBrick(seedsNeighborhood->avg_);
                min = normToBrick(seedsNeighborhood->min_);
                max = normToBrick(seedsNeighborhood->max_);
            }

            RandomWalkerSeedsBrick seeds(seedBufferDimensions, voxelToSeedTransform, foregroundSeeds, backgroundSeeds);
            seeds.addNeighborhoodBorderSeeds(*seedsNeighborhood);
            return seeds;
        } else {
            tgt::svec3 seedBufferDimensions = outputNode.voxelDimensions() / outputNode.scale();
            tgt::mat4 voxelToSeedTransform = tgt::mat4::createScale(tgt::vec3(1.0f / outputNode.scale()));
            RandomWalkerSeedsBrick seeds(seedBufferDimensions, voxelToSeedTransform, foregroundSeeds, backgroundSeeds);
            return seeds;
        }
    }();
    if(stop) {
        return NO_BRICK_ADDRESS;
    }

    tgt::svec3 walkerBlockDim = seeds.bufferDimensions();

    size_t numVoxels = tgt::hmul(walkerBlockDim);
    size_t numSeeds = seeds.getNumSeeds();
    size_t systemSize = numVoxels - numSeeds;

    // Note: outputNode is used here for the region specification only!
    BrickNeighborhood inputNeighborhood = BrickNeighborhood::fromNode(outputNode, outputNode.level_, inputRoot, brickDataSize, inputPoolManager);

    auto volIndexToRow = seeds.generateVolumeToRowsTable();

    std::vector<float> initialization(systemSize, 0.0f);
    if(seedsNeighborhood) {
        auto& neighborhood = *seedsNeighborhood;
        VRN_FOR_EACH_VOXEL(pos, tgt::svec3(0), neighborhood.data_.getDimensions()) {
            size_t logicalIndex = volumeCoordsToIndex(pos, walkerBlockDim);
            if (!seeds.isSeedPoint(logicalIndex)) {
                initialization[volIndexToRow[logicalIndex]] = neighborhood.data_.voxel(pos);
            }
        }
    }

    // No way to decide between foreground and background
    if(numSeeds == 0) {
        avg = 0xffff/2;
        min = avg;
        max = avg;
        for(int i=0; i<numVoxels; ++i) {
            histogram.addSample(0.5f);
        }
        return NO_BRICK_ADDRESS;
    }

    auto solution = tgt::make_unique<float[]>(systemSize);
    std::fill_n(solution.get(), systemSize, 0.5f);

    EllpackMatrix<float> mat;
    mat.setDimensions(systemSize, systemSize, 7);
    mat.initializeBuffers();

    float beta = 0.5f;
    float minWeight = 1.f / pow(10.f, static_cast<float>(input.minWeight_));

    RandomWalkerEdgeWeightIntensity edgeWeightFun(tgt::vec2(0.0f, 1.0f), beta, minWeight);

    auto rwInput = preprocessImageForRandomWalker(inputNeighborhood.data_);
    RandomWalkerVoxelAccessorBrick voxelAccessor(rwInput);

    auto vec = std::vector<float>(systemSize, 0.0f);

    VRN_FOR_EACH_VOXEL(pos, tgt::ivec3(0), tgt::ivec3(walkerBlockDim)) {
        processVoxelWeights(pos, &seeds, mat, vec.data(), volIndexToRow.data(), voxelAccessor, walkerBlockDim, minWeight);
    }

    for(int i=0; i<10; ++i) {
        int iterations;
        {
            //std::lock_guard<std::mutex> guard(clMutex);
            iterations = input.blas_->sSpConjGradEll(mat, vec.data(), solution.get(), initialization.data(),
                input.precond_, input.errorThreshold_, input.maxIterations_);
        }
        if(iterations < input.maxIterations_) {
            break;
        }
        LERRORC("Randomwalker", "MAX ITER NOT SUFFICIENT: " << i);
    }

    const tgt::svec3 brickStart = inputNeighborhood.centerBrickLlf_;
    const tgt::svec3 brickEnd = inputNeighborhood.centerBrickUrb_;
    tgt::svec3 centerBrickSize = brickEnd - brickStart;

    uint64_t sum = 0;

    uint64_t outputBrickAddr = outputPoolManager.allocateBrick();
    {
        OctreeWalkerNodeBrick outputBrick(outputBrickAddr, brickDataSize, outputPoolManager);

        VRN_FOR_EACH_VOXEL(pos, brickStart, brickEnd) {
            size_t logicalIndex = volumeCoordsToIndex(pos, walkerBlockDim);
            float valf;
            if (seeds.isSeedPoint(logicalIndex)) {
                valf = seeds.getSeedValue(logicalIndex);
            } else {
                valf = solution[volIndexToRow[logicalIndex]];
            }
            valf = tgt::clamp(valf, 0.0f, 1.0f);
            uint16_t val = valf*0xffff;

            outputBrick.data_.voxel(pos - brickStart) = val;

            min = std::min(val, min);
            max = std::max(val, max);
            sum += val;

            histogram.addSample(valf);
        }
        avg = sum/tgt::hmul(centerBrickSize);
    }

    if(canSkipChildren(brickToNorm(min), brickToNorm(max))) {
        outputPoolManager.deleteBrick(outputBrickAddr);
        return NO_BRICK_ADDRESS;
    }

    return outputBrickAddr;
}

const std::string BRICK_BUFFER_SUBDIR =      "brickBuffer";
const std::string BRICK_BUFFER_FILE_PREFIX = "buffer_";

struct VolumeOctreeNodeTree {
    VolumeOctreeNodeTree(VolumeOctreeNode* root)
        : root_(root)
    {
    }
    VolumeOctreeNodeTree(VolumeOctreeNodeTree&& other)
        : root_(other.root_)
    {
        other.root_ = nullptr;
    }
    ~VolumeOctreeNodeTree() {
        if(root_) {
            for(int i=0; i<8; ++i) {
                VolumeOctreeNodeTree(root_->children_[i]); // delete recursively
            }
            delete root_;
        }
    }
    VolumeOctreeNode* release() {
        VolumeOctreeNode* ret = root_;
        root_ = nullptr;
        return ret;
    }
    VolumeOctreeNode* root_;
};

const tgt::svec3 OCTREEWALKER_CHILD_POSITIONS[] = {
    tgt::svec3(0,0,0),
    tgt::svec3(1,0,0),
    tgt::svec3(1,1,0),
    tgt::svec3(0,1,0),
    tgt::svec3(0,1,1),
    tgt::svec3(1,1,1),
    tgt::svec3(1,0,1),
    tgt::svec3(0,0,1),
};


OctreeWalker::ComputeOutput OctreeWalker::compute(ComputeInput input, ProgressReporter& progressReporter) const {
    OctreeWalkerOutput invalidResult = OctreeWalkerOutput {
        std::unique_ptr<Volume>(nullptr),
        std::chrono::duration<float>(0),
    };

    progressReporter.setProgress(0.0);

    auto start = clock::now();

    const tgt::svec3 volumeDim = input.octree_.getDimensions();
    const tgt::svec3 brickDim = input.octree_.getBrickDim();
    const size_t brickSize = tgt::hmul(brickDim);
    const size_t numChannels = 1;
    const size_t maxLevel = input.octree_.getNumLevels()-1;

    std::string octreePath = "/home/dominik/nosnapshot/tmp/octreewalkertest/";

    std::string brickPoolPath = tgt::FileSystem::cleanupPath(octreePath + "/" + BRICK_BUFFER_SUBDIR);
    if (!tgt::FileSystem::dirExists(brickPoolPath)) {
        tgt::FileSystem::createDirectoryRecursive(brickPoolPath);
    }

    size_t brickSizeInBytes = brickSize * sizeof(uint16_t);
    OctreeBrickPoolManagerDisk* brickPoolManagerDisk = new OctreeBrickPoolManagerDisk(brickSizeInBytes,
            VoreenApplication::app()->getCpuRamLimit(), brickPoolPath, BRICK_BUFFER_FILE_PREFIX);

    std::unique_ptr<OctreeBrickPoolManagerBase> brickPoolManager(brickPoolManagerDisk);
    tgt::ScopeGuard brickPoolManagerDeinitializer([&brickPoolManager] () {
        brickPoolManager->deinitialize();
    });

    brickPoolManager->initialize(brickSizeInBytes);

    brickPoolManagerDisk->setRAMLimit(1UL * 1024 * 1024 * 1024); //TODO make configurable

    struct NodeToProcess {
        const VolumeOctreeNode* inputNode;
        VolumeOctreeNode* outputNode;
        tgt::svec3 llf;
        tgt::svec3 urb;
    };

    std::vector<NodeToProcess> nodesToProcess;
    VolumeOctreeNode* newRootNode = new VolumeOctreeNodeGeneric<1>(brickPoolManager->allocateBrick(), true);
    nodesToProcess.push_back(
        NodeToProcess {
            input.octree_.getRootNode(),
            newRootNode,
            tgt::svec3::zero,
            volumeDim
        }
    );
    VolumeOctreeNodeTree tree(newRootNode);
    OctreeWalkerNode outputRootNode(*newRootNode, maxLevel, tgt::svec3(0), volumeDim);

    OctreeWalkerNode inputRoot(*input.octree_.getRootNode(), input.octree_.getActualTreeDepth()-1, tgt::svec3(0), input.octree_.getDimensions());

    uint16_t globalMin = 0xffff;
    uint16_t globalMax = 0;

    Histogram1D histogram(0.0, 1.0, 256);

    auto rwm = input.volume_.getRealWorldMapping();

    PointSegmentListGeometryVec3 foregroundSeeds;
    PointSegmentListGeometryVec3 backgroundSeeds;
    getSeedListsFromPorts(input.foregroundGeomSeeds_, foregroundSeeds);
    getSeedListsFromPorts(input.backgroundGeomSeeds_, backgroundSeeds);

    std::mutex clMutex;
#ifdef VRN_OCTREEWALKER_USE_OMP
    LINFO("Using parallel octree walker variant.");
#else
    LINFO("Using sequential octree walker variant.");
#endif


    //auto vmm = input.volume_.getDerivedData<VolumeMinMax>();
    //tgt::vec2 intensityRange(vmm->getMin(), vmm->getMax());

    // Level order iteration => Previos level is always available
    for(int level = maxLevel; level >=0; --level) {
        // Note in the following that 1/4 seems to better represent the actual progress (rather than 1/8).
        // This may be due to the fact that the actual work we have to do happens on the (2D!) _surface_
        // of objects in the volume.
        float progressBegin = 1.0/(1 << (2 * (level + 1))); // 1/4 ^ (level+1 => next level)
        float progressEnd = 1.0/(1 << (2 * (level))); // 1/4 ^ (level)
        SubtaskProgressReporter levelProgress(progressReporter, tgt::vec2(progressBegin, progressEnd));

        LINFO("Level " << level << ": " << nodesToProcess.size() << " Nodes to process.");

        std::vector<NodeToProcess> nextNodesToProcess;

        const int numNodes = nodesToProcess.size();
        ThreadedTaskProgressReporter parallelProgress(levelProgress, numNodes);
        bool aborted = false;
#ifdef VRN_OCTREEWALKER_USE_OMP
        int numThreads = std::max(8 /* at least 8 threads for 8 children per node */, omp_get_num_procs());
#pragma omp parallel for num_threads(numThreads) schedule(dynamic, 1) // Schedule: Process nodes/bricks locally to utilize brick cache
#endif
        for (int nodeId = 0; nodeId < numNodes; ++nodeId) {
#ifdef VRN_OCTREEWALKER_USE_OMP
            if(aborted) {
                continue;
            }
#endif
            auto& node = nodesToProcess[nodeId];
            tgtAssert(node.inputNode, "No input node");

            uint16_t min = 0xffff;
            uint16_t max = 0;
            uint16_t avg = 0xffff/2;
            uint64_t newBrickAddr;
            {
                tgtAssert(node.inputNode->hasBrick(), "No Brick");

                OctreeWalkerNode outputNode(*node.outputNode, level, node.llf, node.urb);
                newBrickAddr = processOctreeBrick(input, outputNode, histogram, min, max, avg, *brickPoolManager, level == maxLevel ? nullptr : &outputRootNode, inputRoot, foregroundSeeds, backgroundSeeds, clMutex);
            }

            globalMin = std::min(globalMin, min);
            globalMax = std::max(globalMax, max);

            auto genericNode = dynamic_cast<VolumeOctreeNodeGeneric<1>*>(node.outputNode);
            tgtAssert(genericNode, "Failed dynamic_cast");
            genericNode->avgValues_[0] = avg;
            genericNode->minValues_[0] = min;
            genericNode->maxValues_[0] = max;

            node.outputNode->setBrickAddress(newBrickAddr);
            if(newBrickAddr != NO_BRICK_ADDRESS && !node.inputNode->isLeaf() /* TODO handle early leaf (with current octree architecture not possible) */) {
                tgt::svec3 childBrickSize = brickDim * (1UL << (level-1));
                for(auto child : OCTREEWALKER_CHILD_POSITIONS) {
                    const size_t childId = volumeCoordsToIndex(child, tgt::svec3::two);
                    VolumeOctreeNode* inputChildNode = node.inputNode->children_[childId];
                    tgtAssert(inputChildNode, "No child node");

                    VolumeOctreeNode* outputChildNode;
                    if(inputChildNode->inVolume()) {
                        outputChildNode = new VolumeOctreeNodeGeneric<1>(NO_BRICK_ADDRESS, true);

                        tgt::svec3 start = node.llf + childBrickSize * child;
                        tgt::svec3 end = tgt::min(start + childBrickSize, volumeDim);
                        #pragma omp critical
                        {
                            nextNodesToProcess.push_back(
                                    NodeToProcess {
                                    inputChildNode,
                                    outputChildNode,
                                    start,
                                    end
                                }
                            );
                        }
                    } else {
                        outputChildNode = new VolumeOctreeNodeGeneric<1>(NO_BRICK_ADDRESS, false);
                    }
                    node.outputNode->children_[childId] = outputChildNode;
                }
            }
            if(parallelProgress.reportStepDone()) {
#ifdef VRN_OCTREEWALKER_USE_OMP
                #pragma omp critical
                {
                    aborted = true;
                }
#else
                aborted = true;
                break;
#endif
            }
        }
        if(aborted) {
            throw boost::thread_interrupted();
        }

        // Make sure to hit LRU cache: Go from back to front in next iteration
        std::reverse(nextNodesToProcess.begin(), nextNodesToProcess.end());

        nodesToProcess = nextNodesToProcess;
    }

    brickPoolManagerDeinitializer.dismiss();
    auto output = tgt::make_unique<Volume>(new VolumeOctree(tree.release(), brickPoolManager.release(), brickDim, input.octree_.getDimensions(), numChannels), &input.volume_);

    float min = static_cast<float>(globalMin)/0xffff;
    float max = static_cast<float>(globalMax)/0xffff;
    output->addDerivedData(new VolumeMinMax(min, max, min, max));
    output->addDerivedData(new VolumeHistogramIntensity(histogram));
    output->setRealWorldMapping(RealWorldMapping(1.0, 0.0f, "Probability"));
    auto finish = clock::now();
    return ComputeOutput {
        std::move(output), finish - start,
    };
}

void OctreeWalker::processComputeOutput(ComputeOutput output) {
    if (output.volume_) {
        LINFO("Total runtime: " << output.duration_.count() << " sec");
    } else {
        LERROR("Failed to compute Random Walker solution");
    }
    outportProbabilities_.setData(output.volume_.release());
}

const VoreenBlas* OctreeWalker::getVoreenBlasFromProperties() const {

#ifdef VRN_MODULE_OPENMP
    if (conjGradImplementation_.isSelected("blasMP")) {
        return &voreenBlasMP_;
    }
#endif
#ifdef VRN_MODULE_OPENCL
    if (conjGradImplementation_.isSelected("blasCL")) {
        return &voreenBlasCL_;
    }
#endif

    return &voreenBlasCPU_;
}


void OctreeWalker::updateGuiState() {
    //bool useTransFunc = enableTransFunc_.get();
    //edgeWeightTransFunc_.setVisibleFlag(useTransFunc);
    //edgeWeightBalance_.setVisibleFlag(useTransFunc);
}

}   // namespace
