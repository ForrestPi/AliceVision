// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "aliceVision/sfm/SfMData.hpp"
#include "aliceVision/sfm/sfmDataIO.hpp"
#include "aliceVision/image/io.hpp"
#include "aliceVision/stl/stl.hpp"

#include <boost/progress.hpp>

namespace aliceVision {
namespace sfm {

using namespace aliceVision::geometry;
using namespace aliceVision::camera;
using namespace aliceVision::image;

bool SfMData::operator==(const SfMData& other) const {

  // Views
  if(views.size() != other.views.size())
    return false;

  for(Views::const_iterator it = views.begin(); it != views.end(); ++it)
  {
      const View& view1 = *(it->second.get());
      const View& view2 = *(other.views.at(it->first).get());
      if(!(view1 == view2))
        return false;

      // Image paths
      if(s_root_path + view1.getImagePath() != other.s_root_path + view2.getImagePath())
        return false;
  }

  // Poses
  if((_poses != other._poses))
    return false;

  // Rigs
  if(_rigs != other._rigs)
    return false;

  // Intrinsics
  if(intrinsics.size() != other.intrinsics.size())
    return false;

  Intrinsics::const_iterator it = intrinsics.begin();
  Intrinsics::const_iterator otherIt = other.intrinsics.begin();
  for(; it != intrinsics.end() && otherIt != other.intrinsics.end(); ++it, ++otherIt)
  {
      // Index
      if(it->first != otherIt->first)
        return false;

      // Intrinsic
      camera::IntrinsicBase& intrinsic1 = *(it->second.get());
      camera::IntrinsicBase& intrinsic2 = *(otherIt->second.get());
      if(!(intrinsic1 == intrinsic2))
        return false;
  }

  // Points IDs are not preserved
  if(structure.size() != other.structure.size())
    return false;

  Landmarks::const_iterator landMarkIt = structure.begin();
  Landmarks::const_iterator otherLandmarkIt = other.structure.begin();
  for(; landMarkIt != structure.end() && otherLandmarkIt != other.structure.end(); ++landMarkIt, ++otherLandmarkIt)
  {
      // Points IDs are not preserved
      // Landmark
      const Landmark& landmark1 = landMarkIt->second;
      const Landmark& landmark2 = otherLandmarkIt->second;
      if(!(landmark1 == landmark2))
        return false;
  }

  // Control points
  if(control_points != other.control_points)
    return false;

  // Root path can be reseted during exports

  return true;

}

std::set<IndexT> SfMData::getValidViews() const
{
  std::set<IndexT> valid_idx;
  for (Views::const_iterator it = views.begin();
    it != views.end(); ++it)
  {
    const View * v = it->second.get();
    if (IsPoseAndIntrinsicDefined(v))
    {
      valid_idx.insert(v->getViewId());
    }
  }
  return valid_idx;
}

std::set<IndexT> SfMData::getReconstructedIntrinsics() const
{
  std::set<IndexT> valid_idx;
  for (Views::const_iterator it = views.begin();
    it != views.end(); ++it)
  {
    const View * v = it->second.get();
    if (IsPoseAndIntrinsicDefined(v))
    {
      valid_idx.insert(v->getIntrinsicId());
    }
  }
  return valid_idx;
}

void SfMData::setPose(const View& view, const geometry::Pose3& absolutePose)
{
  const bool knownPose = existsPose(view);
  Pose3& viewPose = _poses[view.getPoseId()];

  // view is not part of a rig
  if(!view.isPartOfRig())
  {
    viewPose = absolutePose;
    return;
  }

  // view is part of a rig
  const Rig& rig = _rigs.at(view.getRigId());
  RigSubPose& subPose = getRigSubPose(view);

  if(!rig.isInitialized())
  {
    // rig not initialized

    subPose.status = ERigSubPoseStatus::ESTIMATED; // sub-pose initialized
    subPose.pose = Pose3();  // the first sub-pose is set to identity
    viewPose = absolutePose; // so the pose of the rig is the same than the pose
  }
  else
  {
    if(knownPose)
    {
      // rig has a Pose (at least one image of the rig is localized), RigSubPose not initialized

      if(subPose.status != ERigSubPoseStatus::UNINITIALIZED)
      {
        throw std::logic_error("Can't set the pose of an already initialized rig sub-pose");
      }

      // initialize sub-pose
      subPose.status = ERigSubPoseStatus::ESTIMATED;

      // convert absolute pose to RigSubPose
      subPose.pose = absolutePose * viewPose.inverse();

    }
    else
    {
      // rig has no Pose but RigSubPose is known

      if(subPose.status == ERigSubPoseStatus::UNINITIALIZED)
      {
        throw std::logic_error("Can't set the pose of an uninitialized rig sub-pose when rig pose is unknown");
      }

      //convert absolute pose to rig Pose
      viewPose = subPose.pose.inverse() * absolutePose;
    }
  }
}

/// Find the color of the SfMData Landmarks/structure
bool ColorizeTracks( SfMData & sfm_data )
{
  // Colorize each track
  //  Start with the most representative image
  //    and iterate to provide a color to each 3D point

  std::vector<Vec3> vec_3dPoints;
  std::vector<Vec3> vec_tracksColor;

  boost::progress_display my_progress_bar(sfm_data.GetLandmarks().size(),
                                     std::cout,
                                     "\nCompute scene structure color\n");

  vec_3dPoints.resize(sfm_data.GetLandmarks().size());

  //Build a list of contiguous index for the trackIds
  std::map<IndexT, IndexT> trackIds_to_contiguousIndexes;
  IndexT cpt = 0;
  for (Landmarks::const_iterator it = sfm_data.GetLandmarks().begin();
    it != sfm_data.GetLandmarks().end(); ++it, ++cpt)
  {
    trackIds_to_contiguousIndexes[it->first] = cpt;
    vec_3dPoints[cpt] = it->second.X;
  }

  // The track list that will be colored (point removed during the process)
  std::set<IndexT> remainingTrackToColor;
  std::transform(sfm_data.GetLandmarks().begin(), sfm_data.GetLandmarks().end(),
    std::inserter(remainingTrackToColor, remainingTrackToColor.begin()),
    stl::RetrieveKey());
  
  while( !remainingTrackToColor.empty() )
  {
    // Find the most representative image (for the remaining 3D points)
    //  a. Count the number of observation per view for each 3Dpoint Index
    //  b. Sort to find the most representative view index

    std::map<IndexT, IndexT> map_IndexCardinal; // ViewId, Cardinal
    for (std::set<IndexT>::const_iterator
      iterT = remainingTrackToColor.begin();
      iterT != remainingTrackToColor.end();
      ++iterT)
    {
      const size_t trackId = *iterT;
      const Observations & observations = sfm_data.GetLandmarks().at(trackId).observations;
      for( Observations::const_iterator iterObs = observations.begin();
        iterObs != observations.end(); ++iterObs)
      {
        const size_t viewId = iterObs->first;
        if (map_IndexCardinal.find(viewId) == map_IndexCardinal.end())
          map_IndexCardinal[viewId] = 1;
        else
          ++map_IndexCardinal[viewId];
      }
    }

    // Find the View index that is the most represented
    std::vector<IndexT> vec_cardinal;
    std::transform(map_IndexCardinal.begin(),
      map_IndexCardinal.end(),
      std::back_inserter(vec_cardinal),
      stl::RetrieveValue());
    using namespace stl::indexed_sort;
    std::vector< sort_index_packet_descend< IndexT, IndexT> > packet_vec(vec_cardinal.size());
    sort_index_helper(packet_vec, &vec_cardinal[0], 1);

    // First image index with the most of occurrence
    std::map<IndexT, IndexT>::const_iterator iterTT = map_IndexCardinal.begin();
    std::advance(iterTT, packet_vec[0].index);
    const size_t view_index = iterTT->first;
    const View * view = sfm_data.GetViews().at(view_index).get();
    const std::string sView_filename = stlplus::create_filespec(sfm_data.s_root_path,
      view->getImagePath());
    Image<RGBColor> image;
    if(!ReadImage(sView_filename.c_str(), &image))
    {
      ALICEVISION_LOG_WARNING("Unable to read image: " << sView_filename);
      return false;
    }

    // Iterate through the remaining track to color
    // - look if the current view is present to color the track
    std::set<IndexT> set_toRemove;
    for (std::set<IndexT>::const_iterator
      iterT = remainingTrackToColor.begin();
      iterT != remainingTrackToColor.end();
      ++iterT)
    {
      const size_t trackId = *iterT;
      const Observations & observations = sfm_data.GetLandmarks().at(trackId).observations;
      Observations::const_iterator it = observations.find(view_index);

      if (it != observations.end())
      {
        // Color the track
        Vec2 pt = it->second.x;
        // Clamp the pixel position if the feature/marker center is outside the image.
        pt.x() = clamp(pt.x(), 0.0, double(image.Width()-1));
        pt.y() = clamp(pt.y(), 0.0, double(image.Height()-1));
        sfm_data.structure.at(trackId).rgb = image(pt.y(), pt.x());
        set_toRemove.insert(trackId);
        ++my_progress_bar;
      }
    }
    // Remove colored track
    for (std::set<IndexT>::const_iterator iter = set_toRemove.begin();
      iter != set_toRemove.end(); ++iter)
    {
      remainingTrackToColor.erase(*iter);
    }
  }
  return true;
}

} // namespace sfm
} // namespace aliceVision
