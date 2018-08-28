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

#include <cstdio>
#include <string>
#define TGT_ASSERT_H
#define tgtAssert(cond, text) assert((cond))
namespace tgt{
    struct FileSystem {
        static void deleteFile(const std::string& name) {
            std::remove(name.c_str());
        };
    };
}
#include "tgt/vector.h"
#include "kdtree.h"

#include <random>
#include <iostream>


template<typename T>
struct ExampleElement {
    typedef T CoordType;
    const tgt::Vector3<CoordType>& getPos() const {
        return pos_;
    }

    ExampleElement(tgt::Vector3<T> pos, int data)
        : pos_(pos)
        , data_(data)
    {
    }
    tgt::Vector3<T> pos_;
    int data_;
};

template<typename T>
bool test(int numElements, std::function<T()> random) {
    voreen::static_kdtree::ElementArrayBuilder<ExampleElement<T>> builder("elms.tmp");


    auto query = ExampleElement<T>(tgt::vec3(random(), random(), random()), -1);
    int closestData = -1;
    T closestDistSq = std::numeric_limits<T>::max();
    tgt::Vector3<T> closestPos;
    for(int i=0; i<numElements; ++i) {
        ExampleElement<T> elm(tgt::vec3(random(), random(), random()), i);
        T distSq = tgt::distanceSq(elm.getPos(), query.getPos());
        if(distSq < closestDistSq) {
            closestData = i;
            closestDistSq = distSq;
            closestPos = elm.getPos();
        }
        builder.push(elm);
    }

    voreen::static_kdtree::Tree<ExampleElement<T>> tree("tree.bin", std::move(builder));
    std::cout << tree.root().elm_.data_ << std::endl;

    auto result = tree.findNearest(query.getPos());
    if(closestDistSq != result.distSq_) {
        std::cout << "Fail: Expected: " << "distSq: " << +closestDistSq  << " pos: " << closestPos                << " data: " << closestData            << std::endl;
        std::cout << "      Got     : " << "distSq: " << +result.distSq_ << " pos: " << result.element_->getPos() << " data: " << result.element_->data_ << std::endl;
        return false;
    } else {
        std::cout << "Success: Expected: " << "distSq: " << +closestDistSq  << " pos: " << closestPos                << " data: " << closestData            << std::endl;
        std::cout << "         Got     : " << "distSq: " << +result.distSq_ << " pos: " << result.element_->getPos() << " data: " << result.element_->data_ << std::endl;
        return true;
    }
}

int main() {
    std::default_random_engine generator;
    test<float>(10000, [&] () { return std::uniform_real_distribution<float>(0,1)(generator); });
    test<float>(10000, [&] () { return std::uniform_real_distribution<float>(-100000,100000)(generator); });
    test<double>(10000, [&] () { return std::uniform_real_distribution<float>(-1e10,1e10)(generator); });
    test<int>(10000, [&] () { return std::uniform_int_distribution<int>(-10000,10000)(generator); });
    test<int8_t>(10000, [&] () { return std::uniform_int_distribution<int8_t>(-1,1)(generator); });
    test<int16_t>(10000, [&] () { return std::uniform_int_distribution<int16_t>(-50,50)(generator); });
    test<int32_t>(10000, [&] () { return std::uniform_int_distribution<int32_t>(-1000,1000)(generator); });
    test<int64_t>(10000, [&] () { return std::uniform_int_distribution<int64_t>(0,1337)(generator); });
}
