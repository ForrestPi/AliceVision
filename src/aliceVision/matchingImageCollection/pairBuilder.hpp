// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <aliceVision/types.hpp>
#include <aliceVision/sfm/SfMData.hpp>

#include <algorithm>

namespace aliceVision {

/// Generate all the (I,J) pairs of the upper diagonal of the NxN matrix
PairSet exhaustivePairs(const sfm::Views& views, int rangeStart=-1, int rangeSize=0);

/// Generate the pairs that have a distance inferior to the overlapSize
/// Usable to match video sequence
PairSet contiguousWithOverlap(const sfm::Views& views, const size_t overlapSize, int rangeStart=-1, int rangeSize=0);

/// Load a set of PairSet from a file
/// I J K L (pair that link I)
bool loadPairs(
     const std::string &sFileName, // filename of the list file,
     PairSet & pairs,
     int rangeStart=-1,
     int rangeSize=0);

/// Save a set of PairSet to a file (one pair per line)
/// I J
/// I K
bool savePairs(const std::string &sFileName, const PairSet & pairs);

}; // namespace aliceVision
