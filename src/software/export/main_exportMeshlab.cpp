// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "aliceVision/sfm/sfm.hpp"
#include "aliceVision/image/image.hpp"
#include "aliceVision/numeric/numeric.hpp"

#include "dependencies/stlplus3/filesystemSimplified/file_system.hpp"

#include <boost/program_options.hpp>

#include <fstream>

using namespace aliceVision;
using namespace aliceVision::camera;
using namespace aliceVision::geometry;
using namespace aliceVision::image;
using namespace aliceVision::sfm;
namespace po = boost::program_options;

int main(int argc, char **argv)
{
  // command-line parameters

  std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
  std::string sfmDataFilename;
  std::string plyPath;
  std::string outDirectory;

  po::options_description allParams("AliceVision exportMeshlab");

  po::options_description requiredParams("Required parameters");
  requiredParams.add_options()
    ("input,i", po::value<std::string>(&sfmDataFilename)->required(),
      "SfMData file.")
    ("ply", po::value<std::string>(&plyPath)->required(),
      "Ply.")
    ("output,o", po::value<std::string>(&outDirectory)->required(),
      "Output folder.");

  po::options_description logParams("Log parameters");
  logParams.add_options()
    ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
      "verbosity level (fatal,  error, warning, info, debug, trace).");

  allParams.add(requiredParams).add(logParams);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      ALICEVISION_COUT(allParams);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }

  // set verbose level
  system::Logger::get()->setLogLevel(verboseLevel);

  // Create output dir
  if (!stlplus::folder_exists(outDirectory))
    stlplus::folder_create( outDirectory );

  // Read the SfM scene
  SfMData sfm_data;
  if (!Load(sfm_data, sfmDataFilename, ESfMData(VIEWS|INTRINSICS|EXTRINSICS))) {
    std::cerr << std::endl
      << "The input SfMData file \""<< sfmDataFilename << "\" cannot be read." << std::endl;
    return EXIT_FAILURE;
  }

  std::ofstream outfile( stlplus::create_filespec( outDirectory, "sceneMeshlab", "mlp" ).c_str() );

  // Init mlp file
  outfile << "<!DOCTYPE MeshLabDocument>" << outfile.widen('\n')
    << "<MeshLabProject>" << outfile.widen('\n')
    << " <MeshGroup>" << outfile.widen('\n')
    << "  <MLMesh label=\"" << plyPath << "\" filename=\"" << plyPath << "\">" << outfile.widen('\n')
    << "   <MLMatrix44>" << outfile.widen('\n')
    << "1 0 0 0 " << outfile.widen('\n')
    << "0 1 0 0 " << outfile.widen('\n')
    << "0 0 1 0 " << outfile.widen('\n')
    << "0 0 0 1 " << outfile.widen('\n')
    << "</MLMatrix44>" << outfile.widen('\n')
    << "  </MLMesh>" << outfile.widen('\n')
    << " </MeshGroup>" << outfile.widen('\n');

  outfile <<  " <RasterGroup>" << outfile.widen('\n');

  for(Views::const_iterator iter = sfm_data.GetViews().begin();
      iter != sfm_data.GetViews().end(); ++iter)
  {
    const View * view = iter->second.get();
    if (!sfm_data.IsPoseAndIntrinsicDefined(view))
      continue;

    const Pose3 pose = sfm_data.getPose(*view);
    Intrinsics::const_iterator iterIntrinsic = sfm_data.GetIntrinsics().find(view->getIntrinsicId());

    // We have a valid view with a corresponding camera & pose
    const std::string srcImage = stlplus::create_filespec(sfm_data.s_root_path, view->getImagePath());
    const IntrinsicBase * cam = iterIntrinsic->second.get();
    Mat34 P = cam->get_projective_equivalent(pose);

    for ( int i = 1; i < 3 ; ++i)
      for ( int j = 0; j < 4; ++j)
        P(i, j) *= -1.;

    Mat3 R, K;
    Vec3 t;
    KRt_From_P( P, &K, &R, &t);

    const Vec3 optical_center = R.transpose() * t;

    outfile
      << "  <MLRaster label=\"" << stlplus::filename_part(view->getImagePath()) << "\">" << std::endl
      << "   <VCGCamera TranslationVector=\""
      << optical_center[0] << " "
      << optical_center[1] << " "
      << optical_center[2] << " "
      << " 1 \""
      << " LensDistortion=\"0 0\""
      << " ViewportPx=\"" << cam->w() << " " << cam->h() << "\""
      << " PixelSizeMm=\"" << 1  << " " << 1 << "\""
      << " CenterPx=\"" << cam->w() / 2.0 << " " << cam->h() / 2.0 << "\""
      << " FocalMm=\"" << (double)K(0, 0 )  << "\""
      << " RotationMatrix=\""
      << R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << " 0 "
      << R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << " 0 "
      << R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << " 0 "
      << "0 0 0 1 \"/>"  << std::endl;

    // Link the image plane
    outfile << "   <Plane semantic=\"\" fileName=\"" << srcImage << "\"/> "<< std::endl;
    outfile << "  </MLRaster>" << std::endl;
  }
  outfile << "   </RasterGroup>" << std::endl
    << "</MeshLabProject>" << std::endl;

  outfile.close();

  return EXIT_SUCCESS;
}
