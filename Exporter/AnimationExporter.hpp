#ifndef ANIMATION_EXPORTER_HPP
#define ANIMATION_EXPORTER_HPP


struct KeyFrame
{
  MTime   time;
  MMatrix transformation;
};

typedef std::vector<KeyFrame> Track;

class AnimationExporter
{
public:
  AnimationExporter(ChunkIo& writer);

  MStatus do_export();
  bool  get_track_for_transform(Track& track, const std::string& transform_name) const;
  bool  is_animated(const std::string& transform_name) const;
private:

  typedef std::map<std::string, Track> StringTrackMap;

  typedef std::multimap<MTime, uint32_t> TimeToPathIdxMap;

  MStatus collect_transform_paths();
  void create_time_to_idx_mapping();
  MStatus write_animation();
  MStatus collect_transforms();

  std::vector<MDagPath> transform_paths_;
  TimeToPathIdxMap time_to_path_idx_map_;

  uint32_t fps_;
  MTime start_;
  MTime end_;
  StringTrackMap tracks_;

  ChunkIo& writer_;
};

#endif
