#include "stdafx.h"
#include "AnimationExporter.hpp"
#include "ExporterUtils.hpp"
#include <celsus/DX10Utils.hpp>

namespace
{
  const uint32_t kConstantFps = 30;

  uint32_t getFps()
  {
    switch(MTime::uiUnit())
    {
    case MTime::kSeconds:       return 1;
    case MTime::kMilliseconds:  return 1000;
    case MTime::kGames:         return 15;
    case MTime::kFilm:          return 24;
    case MTime::kPALFrame:      return 25;
    case MTime::kNTSCFrame:     return 30;
    case MTime::kShowScan:      return 48;
    case MTime::kPALField:      return 50;
    case MTime::kNTSCField:     return 60;
    default:                    return 1;
      // We are ignoring the following time-units.
      //case MTime::kHours:       return 1;
      //case MTime::kMinutes:     return 1;
    }
  }

}

AnimationExporter::AnimationExporter(ChunkIo& writer)
  : writer_(writer)
{
}

bool get_start_end_time(double& start_time, double& end_time, const MDagPath& path)
{
  std::vector<double> keyframes;
  MPlugArray animatedPlugs;
  MAnimUtil::findAnimatedPlugs(path, animatedPlugs);
  for (unsigned int i = 0; i < animatedPlugs.length(); ++i) {

    MObjectArray curves;
    MAnimUtil::findAnimation(animatedPlugs[i], curves);
    for (unsigned int j = 0; j < curves.length(); ++j) {

      MFnAnimCurve curve(curves[j]);
      for (unsigned int k = 0; k < curve.numKeys(); ++k) {
        keyframes.push_back(curve.time(k).as(MTime::kSeconds));
      }
    }
  }
  if (keyframes.empty()) {
    return false;
  }

  start_time = keyframes.front();
  end_time = keyframes.front();

  for (std::size_t i = 0, e = keyframes.size(); i < e; ++i) {
    start_time = std::min<double>(start_time, keyframes[i]);
    end_time = std::max<double>(end_time, keyframes[i]);
  }
  return true;
}
/*
// runs over all animated joints, and extracts the keyframe-times:
// finds the animated plugs, and gets all curve-times.
void AnimationExporter::extractKeyFrameTimes(std::set<MTime>& jointTimes, const MDagPath& path)
{
  double start_time, end_time;
  if (get_start_end_time(start_time, end_time, path)) {
    const double animation_length = end_time - start_time;
    const double inc = animation_length / (double)kConstantFps;

    for (double cur_time = start_time; cur_time <= end_time; cur_time += inc) {
      jointTimes.insert(MTime(cur_time, MTime::kSeconds));
    }
  }
}
*/

void AnimationExporter::create_time_to_idx_mapping()
{

  // create a mapping of "time -> path_idx" for each time in the
  // animated sequence

  for (uint32_t i = 0; i < transform_paths_.size(); ++i) {

    const MDagPath& cur_path = transform_paths_[i];
    double start_time, end_time;

    if (get_start_end_time(start_time, end_time, cur_path)) {

      const double animation_length = end_time - start_time;
      const double inc = animation_length / (double)kConstantFps;

      for (double cur_time = start_time; cur_time <= end_time; cur_time += inc) {
        time_to_path_idx_map_.insert(std::make_pair(MTime(cur_time, MTime::kSeconds), i));
      }
    } else {
      time_to_path_idx_map_.insert(std::make_pair(MTime(0.0), i));
    }
  }
}

MStatus AnimationExporter::collect_transform_paths()
{
  // We only export animations for transforms
  for( MItDag it(MItDag::kDepthFirst); !it.isDone(); it.next() ) {
    MDagPath dag_path;
    CONTINUE_ON_ERROR_MSTATUS(it.getPath(dag_path));
    if (dag_path.apiType() == MFn::kTransform) {
      transform_paths_.push_back(dag_path);
    } 
  }
  return MS::kSuccess;
}

MStatus AnimationExporter::collect_transforms()
{
  // Loop over all the saved (time, path_idx) pairs, and at each time get
  // the transform for the path

  const MTime initialTime = MAnimControl::currentTime();

  for (TimeToPathIdxMap::const_iterator it = time_to_path_idx_map_.begin(); it != time_to_path_idx_map_.end(); ++it) {
    if (MAnimControl::currentTime() != it->first) {
      MAnimControl::setCurrentTime(it->first);
    }
    const std::string path_name(strip_pipes(transform_paths_[it->second].fullPathName().asChar()));
    Track& currentTrack = tracks_[path_name];
    const MDagPath& current_path = transform_paths_[it->second];
    KeyFrame newKeyFrame;
    newKeyFrame.time = it->first;

    MFnTransform transform(current_path);
    newKeyFrame.transformation = transform.transformation().asMatrix();
    currentTrack.push_back(newKeyFrame);
  }
  // back to initial time
  MAnimControl::setCurrentTime(initialTime);

  return MS::kSuccess;
}

MStatus AnimationExporter::do_export()
{
  RETURN_ON_ERROR_MSTATUS(collect_transform_paths());

  create_time_to_idx_mapping();

  if (time_to_path_idx_map_.size() == 0) {
    return MS::kSuccess;
  }

  fps_ = getFps();
  start_ = time_to_path_idx_map_.begin()->first; // start-time
  end_ = time_to_path_idx_map_.rbegin()->first; // end-time

  RETURN_ON_ERROR_MSTATUS(collect_transforms());
  RETURN_ON_ERROR_MSTATUS(write_animation());

  return MS::kSuccess;
}

MStatus AnimationExporter::write_animation()
{
  // All times are converted to seconds when exported
  SCOPED_CHUNK(writer_, ChunkHeader::Animation);
  RETURN_ON_ERROR_BOOL(writer_.write_generic<uint32_t>(fps_));
  const double fps = fps_;
  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)(start_.value() / fps)));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)(end_.value() / fps)));

  RETURN_ON_ERROR_BOOL(writer_.write_generic<uint32_t>((uint32_t)tracks_.size()));
  for (StringTrackMap::iterator it_track = tracks_.begin(); it_track != tracks_.end(); ++it_track) {

    // write track name
    const std::string track_name = it_track->first;
    RETURN_ON_ERROR_BOOL(writer_.write_string(track_name.c_str()));

    // write keys for track
    const uint32_t track_count = (uint32_t)it_track->second.size();
    const bool export_static = track_count == 1;
    if (export_static) {
      std::cout << "Exporting transform " << track_name << " as static" << std::endl;
    }
    RETURN_ON_ERROR_BOOL(writer_.write_generic<uint32_t>(track_count));

    for (Track::iterator it_key = it_track->second.begin(); it_key != it_track->second.end(); ++it_key) {
      const KeyFrame& cur_Track = *it_key;
      RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)(cur_Track.time.value() / fps)));

      D3DXVECTOR3 trans, scale;
      D3DXQUATERNION rot;
      if (export_static) {
        trans = kVec3Zero;
        scale = kVec3One;
        rot = kQuatId;
      } else {
        decompose_matrix(trans, rot, scale, cur_Track.transformation);
      }
      RETURN_ON_ERROR_BOOL(writer_.write_generic<D3DXVECTOR3>(trans));
      RETURN_ON_ERROR_BOOL(writer_.write_generic<D3DXQUATERNION>(rot));
      RETURN_ON_ERROR_BOOL(writer_.write_generic<D3DXVECTOR3>(scale));
    }
  }
  return MS::kSuccess;
}

bool AnimationExporter::get_track_for_transform(Track& track, const std::string& transform_name) const
{
  StringTrackMap::const_iterator it = tracks_.find(transform_name);
  if (it == tracks_.end()) {
    return false;
  }
  track = it->second;
  return true;
}

bool AnimationExporter::is_animated(const std::string& transform_name) const
{
  StringTrackMap::const_iterator it = tracks_.find(transform_name);
  if (it == tracks_.end()) {
    return false;
  }
  return it->second.size() > 1;
}
