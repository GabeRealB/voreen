/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2017 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
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

#include "similaritymatrix.h"

#include "ensembledataset.h"
#include "../utils/ensemblehash.h"

namespace voreen {

SimilarityMatrix::SimilarityMatrix()
    : size_(0)
{
}

SimilarityMatrix::SimilarityMatrix(size_t size)
    : data_((size+1) * size/2, 0.0f)
    , size_(size)
{
}

SimilarityMatrix::SimilarityMatrix(const SimilarityMatrix& other)
    : data_(other.data_)
    , size_(other.size_)
{
}

SimilarityMatrix::SimilarityMatrix(SimilarityMatrix&& other)
    : data_(std::move(other.data_))
    , size_(other.size_)
{
}

SimilarityMatrix::~SimilarityMatrix()
{
}

SimilarityMatrix& SimilarityMatrix::operator=(const SimilarityMatrix& other) {
    data_ = other.data_;
    size_ = other.size_;

    return *this;
}

SimilarityMatrix& SimilarityMatrix::operator=(SimilarityMatrix&& other) {
    data_ = std::move(other.data_);
    size_ = other.size_;

    return *this;
}

size_t SimilarityMatrix::getSize() const {
    return size_;
}

float& SimilarityMatrix::operator() (size_t i, size_t j) {
    return data_[index(i, j)];
}

float SimilarityMatrix::operator() (size_t i, size_t j) const {
    return data_[index(i, j)];
}

void SimilarityMatrix::serialize(Serializer& s) const {
    s.serialize("data", data_);
    s.serialize("size", size_);
}

void SimilarityMatrix::deserialize(Deserializer& s) {
    s.deserialize("data", data_);
    s.deserialize("size", size_);
}

SimilarityMatrixList::SimilarityMatrixList()
{
}

SimilarityMatrixList::SimilarityMatrixList(const EnsembleDataset& dataset)
    : ensembleHash_(EnsembleHash(dataset).getHash())
{
    for(const std::string& channel : dataset.getCommonChannels()) {
        matrices_.insert(std::make_pair(channel, SimilarityMatrix(dataset.getTotalNumTimeSteps())));
    }
}

SimilarityMatrixList::SimilarityMatrixList(const SimilarityMatrixList& other)
    : matrices_(other.matrices_)
    , ensembleHash_(other.ensembleHash_)
{
}

SimilarityMatrixList::SimilarityMatrixList(SimilarityMatrixList&& other)
    : matrices_(std::move(other.matrices_))
    , ensembleHash_(other.ensembleHash_)
{
}

const std::string& SimilarityMatrixList::getHash() const {
    return ensembleHash_;
}

SimilarityMatrix& SimilarityMatrixList::getSimilarityMatrix(const std::string& channel) {
    notifyPendingDataInvalidation();
    return matrices_.at(channel);
}

void SimilarityMatrixList::serialize(Serializer& s) const {
    s.serialize("matrices", matrices_);
    s.serialize("hash", ensembleHash_);
}

void SimilarityMatrixList::deserialize(Deserializer& s) {
    s.deserialize("matrices", matrices_);
    s.deserialize("hash", ensembleHash_);
}

}   // namespace
