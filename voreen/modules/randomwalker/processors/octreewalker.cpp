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

#include "../solver/randomwalkersolver.h"
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

namespace voreen {

const std::string OctreeWalker::loggerCat_("voreen.RandomWalker.OctreeWalker");
using tgt::vec3;

OctreeWalker::OctreeWalker()
    : AsyncComputeProcessor<OctreeWalkerInput, OctreeWalkerOutput>(),
    inportVolume_(Port::INPORT, "volume.input"),
    inportForegroundSeeds_(Port::INPORT, "geometry.seedsForeground", "geometry.seedsForeground", true),
    inportBackgroundSeeds_(Port::INPORT, "geometry.seedsBackground", "geometry.seedsBackground", true),
    outportProbabilities_(Port::OUTPORT, "volume.probabilities", "volume.probabilities", false),
    usePrevProbAsInitialization_("usePrevProbAsInitialization", "Use Previous Probabilities as Initialization", false, Processor::VALID, Property::LOD_ADVANCED),
    beta_("beta", "Edge Weight Scale: 2^beta", 12, 0, 20),
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
    addProperty(beta_);
    addProperty(minEdgeWeight_);
    beta_.setGroupID("rwparam");
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
        beta_.get(),
        minEdgeWeight_.get(),
        voreenBlas,
        precond,
        errorThresh,
        maxIterations,
        homogeneityThreshold_.get(),
    };
}

namespace {
    struct LoopRecord {
        int iteration;
        int level;
        float scaleFactor;
        tgt::ivec3 workDim;
        size_t numSeeds;
        size_t numForegroundSeeds;
        size_t numBackgroundSeeds;
        tgt::vec2 probabilityRange;
        int numIterations;
        std::chrono::duration<float> timeIteration;
        std::chrono::duration<float> timeSetup;
        std::chrono::duration<float> timeSolving;
        std::chrono::duration<float> timeSeedAnalysis;

        void print() {
            size_t numVoxels = tgt::hmul(workDim);
            std::string cat = "voreen.RandomWalker.OctreeWalker";
            LINFOC(cat, iteration << ". Iteration: level=" << level << ", scaleFactor=" << scaleFactor
                << ", dim=" << workDim);
            LINFOC(cat, "* num voxels: " << numVoxels << ", num seeds:  " << numSeeds << " (ratio: " << (float)numSeeds/tgt::hmul(workDim) << ")");
            LINFOC(cat, "* num unseeded: " << numVoxels-numSeeds);
            LINFOC(cat, "* probability range: " << probabilityRange);
            LINFOC(cat, "* runtime: " << timeIteration.count() << " sec");
            LINFOC(cat, "  - system setup: " << timeSetup.count() << " sec");
            LINFOC(cat, "  - solving: " << timeSolving.count() << " sec"
                << " (iterations: " << numIterations << ")");
            LINFOC(cat, "  - seed analysis: " << timeSeedAnalysis.count() << " sec");
        }
    };
} // namespace anonymous

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

//TODO fix original macro
#define VRN_FOR_EACH_VOXEL2(INDEX, POS, SIZE) \
    for (auto (INDEX) = (POS); (INDEX).z < (SIZE).z; ++(INDEX).z)\
        for ((INDEX).y = (POS).y; (INDEX).y < (SIZE).y; ++(INDEX).y)\
            for ((INDEX).x = (POS).x; (INDEX).x < (SIZE).x; ++(INDEX).x)

struct BrickNeighborhood {
    BrickNeighborhood() = delete;
    BrickNeighborhood(const BrickNeighborhood& other) = delete;
    BrickNeighborhood(BrickNeighborhood&& other) = default;

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

    static BrickNeighborhood empty(tgt::svec3 dimensions, int scale) {
        return BrickNeighborhood {
            VolumeAtomic<float>(tgt::svec3(0)),
            tgt::svec3(0),
            dimensions,
            dimensions,
            tgt::mat4::identity,
            0.0f,
            0.0f,
        };
    }
    static BrickNeighborhood fromNode(const OctreeWalkerNode& current, size_t sampleLevel, const OctreeWalkerNode& root, const tgt::svec3& brickBaseSize, const OctreeBrickPoolManagerBase& brickPoolManager) {
        const tgt::svec3 volumeDim = root.voxelDimensions();

        const tgt::mat4 brickToVoxel = current.brickToVoxel();
        const tgt::mat4 voxelToBrick = current.voxelToBrick();

        const tgt::ivec3 neighborhoodSize = brickBaseSize;
        //const tgt::ivec3 neighborhoodSize = tgt::ivec3(0);

        const tgt::ivec3 brickLlf(0);
        const tgt::ivec3 brickUrb = voxelToBrick.transform(current.urb_);

        const tgt::svec3 voxelLlf = tgt::max(tgt::vec3(0),         brickToVoxel.transform(brickLlf - neighborhoodSize));
        const tgt::svec3 voxelUrb = tgt::min(tgt::vec3(volumeDim), brickToVoxel.transform(brickUrb + neighborhoodSize));

        const tgt::ivec3 regionLlf = voxelToBrick.transform(voxelLlf);
        const tgt::ivec3 regionUrb = voxelToBrick.transform(voxelUrb);

        const tgt::svec3 regionDim = regionUrb - regionLlf;

        VolumeAtomic<float> output(regionDim);

        float min = FLT_MAX;
        float max = FLT_MIN;

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

                tgt::mat4 centerToSampleBrick = node.voxelToBrick() * brickToVoxel;
                VRN_FOR_EACH_VOXEL2(point, blockLlf, blockUrb) {
                    tgt::vec3 samplePos = centerToSampleBrick.transform(point);
                    float val = brick.data_.getVoxelNormalized(samplePos);
                    tgt::vec3 neighborhoodBufferPos = point - regionLlf;
                    output.setVoxelNormalized(val, neighborhoodBufferPos);
                    min = std::min(val, min);
                    max = std::max(val, max);
                }
            } else {
                float val = static_cast<float>(node.node_.getAvgValue())/0xffff;
                min = val;
                max = val;
                VRN_FOR_EACH_VOXEL2(point, blockLlf, blockUrb) {
                    tgt::vec3 neighborhoodBufferPos = point - regionLlf;
                    output.setVoxelNormalized(val, neighborhoodBufferPos);
                }
            }
        }
        return BrickNeighborhood {
            std::move(output),
            -regionLlf,
            -regionLlf+brickUrb,
            regionDim,
            voxelToBrick,
            min,
            max
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
        numSeeds_ = 0;
        seedBuffer_.fill(UNLABELED);

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
                        if (seedBuffer_.voxel(point) == UNLABELED) {
                            seedBuffer_.voxel(point) = label;
                            numSeeds_++;
                        //} else {
                        //    seedBuffer_.voxel(point) = 0.5; //TODO revisit
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
                float val = neighborhood.data_.getVoxelNormalized(seed);
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

    // TODO check conversion here
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

struct RandomWalkerVoxelAccessorBrick : public RandomWalkerVoxelAccessor {
    RandomWalkerVoxelAccessorBrick(const BrickNeighborhood& brick, RealWorldMapping rwm)
        : brick_(brick)
        , rwm_(rwm)
    {
    }
    virtual float voxel(const tgt::svec3& pos) {
        tgt::ivec3 brickPos = pos;// - seedBufferLLFOffset_;
        float normalized = brick_.data_.getVoxelNormalized(brickPos);
        return rwm_.normalizedToRealWorld(normalized);
    }
private:
    const BrickNeighborhood& brick_;
    RealWorldMapping rwm_;
};


static void processOctreeBrick(OctreeWalkerInput& input, OctreeWalkerNode& outputNode, OctreeWalkerNodeBrick& outputBrick, ProgressReporter& progressReporter, Histogram1D& histogram, uint16_t& min, uint16_t& max, uint16_t& avg, const OctreeBrickPoolManagerBase& outputPoolManager, OctreeWalkerNode* outputRoot) {
    //TODO: catch out of memory
    const OctreeBrickPoolManagerBase& inputPoolManager = *input.octree_.getBrickPoolManager();
    const tgt::svec3 brickDataSize = input.octree_.getBrickDim();

    //TODO construct outside and reuse?
    OctreeWalkerNode inputRoot(*input.octree_.getRootNode(), input.octree_.getActualTreeDepth()-1, tgt::svec3(0), input.octree_.getDimensions());

    //TODO: Only copy once
    PointSegmentListGeometryVec3 foregroundSeeds;
    PointSegmentListGeometryVec3 backgroundSeeds;
    getSeedListsFromPorts(input.foregroundGeomSeeds_, foregroundSeeds);
    getSeedListsFromPorts(input.backgroundGeomSeeds_, backgroundSeeds);

    RandomWalkerSeedsBrick seeds = [&] () {
        if(outputRoot) {
            BrickNeighborhood seedsNeighborhood = BrickNeighborhood::fromNode(outputNode, outputNode.level_+1, *outputRoot, brickDataSize, outputPoolManager);
            tgt::svec3 seedBufferDimensions = seedsNeighborhood.data_.getDimensions();
            tgt::mat4 voxelToSeedTransform = seedsNeighborhood.voxelToNeighborhood();

            RandomWalkerSeedsBrick seeds(seedBufferDimensions, voxelToSeedTransform, foregroundSeeds, backgroundSeeds);
            seeds.addNeighborhoodBorderSeeds(seedsNeighborhood);
            return seeds;
        } else {
            tgt::svec3 seedBufferDimensions = outputNode.voxelDimensions() / outputNode.scale();
            tgt::mat4 voxelToSeedTransform = tgt::mat4::createScale(tgt::vec3(1.0f / outputNode.scale()));
            return RandomWalkerSeedsBrick(seedBufferDimensions, voxelToSeedTransform, foregroundSeeds, backgroundSeeds);
        }
    }();

    // Note: outputNode is used here for the region specification only!
    BrickNeighborhood inputNeighborhood = BrickNeighborhood::fromNode(outputNode, outputNode.level_, inputRoot, brickDataSize, inputPoolManager);

    tgt::svec3 walkerBlockDim = seeds.bufferDimensions();

    float* oldSystemSolution = nullptr;
    size_t numVoxels = tgt::hmul(walkerBlockDim);
    size_t numSeeds = seeds.getNumSeeds();
    size_t systemSize = numVoxels - numSeeds;

    // No way to decide between foreground and background
    if(numSeeds == 0) {
        avg = 0xffff/2;
        min = avg;
        max = avg;
        for(int i=0; i<numVoxels; ++i) {
            histogram.addSample(0.5f);
        }
        outputBrick.data_.fill(avg);
        return;
    }

    auto solution = tgt::make_unique<float[]>(systemSize);
    std::fill_n(solution.get(), systemSize, 0.5f);

    EllpackMatrix<float> mat;
    mat.setDimensions(systemSize, systemSize, 7);
    mat.initializeBuffers();

    auto rwm = input.volume_.getRealWorldMapping();
    auto vmm = input.volume_.getDerivedData<VolumeMinMax>();
    tgt::vec2 intensityRange(vmm->getMin(), vmm->getMax());
    //tgt::vec2 intensityRange(rwm.normalizedToRealWorld(inputNeighborhood.min_), rwm.normalizedToRealWorld(inputNeighborhood.max_));
    std::unique_ptr<RandomWalkerEdgeWeight> edgeWeightFun;

    float beta = static_cast<float>(1<<input.beta_);
    float minWeight = 1.f / pow(10.f, static_cast<float>(input.minWeight_));

    edgeWeightFun.reset(new RandomWalkerEdgeWeightIntensity(intensityRange, beta, minWeight));

    std::unique_ptr<RandomWalkerVoxelAccessor> voxelAccessor(new RandomWalkerVoxelAccessorBrick(inputNeighborhood, rwm));

    RandomWalkerWeights edgeWeights(std::move(voxelAccessor), std::move(edgeWeightFun), walkerBlockDim);

    auto vec = std::vector<float>(systemSize, 0.0f);

    auto volIndexToRow = seeds.generateVolumeToRowsTable();

    VRN_FOR_EACH_VOXEL(pos, tgt::ivec3(0), tgt::ivec3(walkerBlockDim)) {
        edgeWeights.processVoxel(pos, &seeds, mat, vec.data(), volIndexToRow.data());
    }

    for(int i=0; i<10; ++i) {
        int iterations = input.blas_->sSpConjGradEll(mat, vec.data(), solution.get(), oldSystemSolution,
            input.precond_, input.errorThreshold_, input.maxIterations_, progressReporter);
        if(iterations < input.maxIterations_) {
            break;
        }
        LERRORC("Randomwalker", "MAX ITER NOT SUFFICIENT: " << i);
    }
    //TODO do something with iterations? do multiple blas runs? (Probably not necessary in most cases due to small bricks (and initialization (another TODO)?))

    uint64_t sum = 0;

    tgt::svec3 start = inputNeighborhood.centerBrickLlf_;
    tgt::svec3 end = inputNeighborhood.centerBrickUrb_;
    tgt::svec3 centerBrickSize = end - start;

    VRN_FOR_EACH_VOXEL(pos, start, end) {
        size_t logicalIndex = volumeCoordsToIndex(pos, walkerBlockDim);
        float valf;
        if (seeds.isSeedPoint(logicalIndex)) {
            valf = seeds.getSeedValue(logicalIndex);
        } else {
            valf = solution[volIndexToRow[logicalIndex]];
        }
        uint16_t val = valf*0xffff;

        outputBrick.data_.voxel(pos - start) = val;

        min = std::min(val, min);
        max = std::max(val, max);
        sum += val;

        histogram.addSample(valf);
    }
    avg = sum/tgt::hmul(centerBrickSize);
}

const std::string BRICK_BUFFER_SUBDIR =      "brickBuffer";
const std::string BRICK_BUFFER_FILE_PREFIX = "buffer_";

template<typename C>
struct DeinitPtr {
    DeinitPtr(C* content)
        : content_(content)
    {
    }
    DeinitPtr(DeinitPtr&& other)
        : content_(other.content_)
    {
        other.content_ = nullptr;
    }
    ~DeinitPtr() {
        if(content_) {
            content_->deinitialize();
            delete content_;
        }
    }
    C* release() {
        C* ret = content_;
        content_ = nullptr;
        return ret;
    }
    C& operator* () {
        return *content_;
    }

    C* operator-> () {
        return content_;
    }
    C* content_;
};

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

    DeinitPtr<OctreeBrickPoolManagerBase> brickPoolManager(brickPoolManagerDisk); //TODO use stackguard instead

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

    uint16_t globalMin = 0xffff;
    uint16_t globalMax = 0;

    Histogram1D histogram(0.0, 1.0, 256);

    // Level order iteration => Previos level is always available
    for(int level = maxLevel; level >=0; --level) {
        float progressBegin = 1.0/(1 << (3 * (level + 1))); // 1/8 ^ (level+1 => next level)
        float progressEnd = 1.0/(1 << (3 * (level))); // 1/8 ^ (level)
        SubtaskProgressReporter levelProgress(progressReporter, tgt::vec2(progressBegin, progressEnd));

        LINFO("Level " << level << ": " << nodesToProcess.size() << " Nodes to process.");

        std::vector<NodeToProcess> nextNodesToProcess;
        int i=0;
        float perNodeProgress = 1.0f/nodesToProcess.size();
        for(auto& node : nodesToProcess) {
            SubtaskProgressReporter progress(levelProgress, tgt::vec2(i*perNodeProgress, (i+1)*perNodeProgress));
            tgtAssert(node.inputNode, "No input node");

            uint16_t min = 0xffff;
            uint16_t max = 0;
            uint16_t avg = 0xffff/2;
            {
                tgtAssert(node.inputNode->hasBrick(), "No Brick");

                OctreeWalkerNodeBrick newBrick(node.outputNode->getBrickAddress(), brickDim, *brickPoolManager);

                OctreeWalkerNode outputNode(*node.outputNode, level, node.llf, node.urb);
                processOctreeBrick(input, outputNode, newBrick, progress, histogram, min, max, avg, *brickPoolManager, level == maxLevel ? nullptr : &outputRootNode);
            }

            globalMin = std::min(globalMin, min);
            globalMax = std::max(globalMax, max);

            auto genericNode = dynamic_cast<VolumeOctreeNodeGeneric<1>*>(node.outputNode);
            tgtAssert(genericNode, "Failed dynamic_cast");
            genericNode->avgValues_[0] = avg;
            genericNode->minValues_[0] = min;
            genericNode->maxValues_[0] = max;

            float range = static_cast<float>(max-min)/0xffff;

            //float fmin = static_cast<float>(min)/0xffff;
            //float fmax = static_cast<float>(max)/0xffff;
            //if(range < input.homogeneityThreshold_ || fmin > 0.6f || fmax < 0.4f)

            if(range < input.homogeneityThreshold_) {
                brickPoolManager->deleteBrick(node.outputNode->getBrickAddress());
                node.outputNode->setBrickAddress(NO_BRICK_ADDRESS);
            } else if(!node.inputNode->isLeaf() /* TODO handle early leaf */) {
                tgt::svec3 childBrickSize = brickDim * (1UL << (level-1));
                VRN_FOR_EACH_VOXEL(child, tgt::svec3::zero, tgt::svec3::two) {
                    const size_t childId = volumeCoordsToIndex(child, tgt::svec3::two);
                    VolumeOctreeNode* inputChildNode = node.inputNode->children_[childId];
                    tgtAssert(inputChildNode, "No child node");

                    VolumeOctreeNode* outputChildNode;
                    if(inputChildNode->inVolume()) {
                        outputChildNode = new VolumeOctreeNodeGeneric<1>(brickPoolManager->allocateBrick(), true);

                        tgt::svec3 start = node.llf + childBrickSize * child;
                        tgt::svec3 end = tgt::min(start + childBrickSize, volumeDim);
                        nextNodesToProcess.push_back(
                                NodeToProcess {
                                inputChildNode,
                                outputChildNode,
                                start,
                                end
                            }
                        );
                    } else {
                        outputChildNode = new VolumeOctreeNodeGeneric<1>(NO_BRICK_ADDRESS, false);
                    }
                    node.outputNode->children_[childId] = outputChildNode;
                }
            }
            progress.setProgress(1.0f);
            ++i;
        }
        nodesToProcess = nextNodesToProcess;
    }


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
