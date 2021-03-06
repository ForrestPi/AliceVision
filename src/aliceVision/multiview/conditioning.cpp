// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "conditioning.hpp"

namespace aliceVision {

// HZ 4.4.4 pag.109
void PreconditionerFromPoints(const Mat &points, Mat3 *T) {
  Vec mean, variance;
  MeanAndVarianceAlongRows(points, &mean, &variance);

  double xfactor = sqrt(2.0 / variance(0));
  double yfactor = sqrt(2.0 / variance(1));

  // If variance is equal to 0.0 set scaling factor to identity.
  // -> Else it will provide nan value (because division by 0).
  if (variance(0) < 1e-8)
    xfactor = mean(0) = 1.0;
  if (variance(1) < 1e-8)
    yfactor = mean(1) = 1.0;

  *T << xfactor, 0,       -xfactor * mean(0),
        0,       yfactor, -yfactor * mean(1),
        0,       0,        1;
}

void PreconditionerFromImageSize(int width, int height, Mat3 *T) {
  // Build the normalization matrix
  double dNorm = 1.0 / sqrt( static_cast<double>(width*height) );

  (*T) = Mat3::Identity();
  (*T)(0,0) = (*T)(1,1) = dNorm;
  (*T)(0,2) = -.5f*width*dNorm;
  (*T)(1,2) = -.5*height*dNorm;
}

void ApplyTransformationToPoints(const Mat &points,
                                 const Mat3 &T,
                                 Mat *transformed_points) {
  const Mat::Index n = points.cols();
  transformed_points->resize(2,n);
  for (Mat::Index i = 0; i < n; ++i) {
    Vec3 in, out;
    in << points(0, i), points(1, i), 1;
    out = T * in;
    (*transformed_points)(0, i) = out(0)/out(2);
    (*transformed_points)(1, i) = out(1)/out(2);
  }
}

void NormalizePointsFromImageSize(const Mat &points,
                      Mat *normalized_points,
                      Mat3 *T,
                      int width,
                      int height) {
  PreconditionerFromImageSize(width, height, T);
  ApplyTransformationToPoints(points, *T, normalized_points);
}


void NormalizePoints(const Mat &points,
                     Mat *normalized_points,
                     Mat3 *T) {
  PreconditionerFromPoints(points, T);
  ApplyTransformationToPoints(points, *T, normalized_points);
}

// Denormalize the results. See HZ page 109.
void UnnormalizerT::Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H)  {
  *H = T2.transpose() * (*H) * T1;
}

// Denormalize the results. See HZ page 109.
void UnnormalizerI::Unnormalize(const Mat3 &T1, const Mat3 &T2, Mat3 *H)  {
  *H = T2.inverse() * (*H) * T1;
}

} // namespace aliceVision
