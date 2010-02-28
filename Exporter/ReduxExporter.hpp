#ifndef REDUX_EXPORTER_HPP
#define REDUX_EXPORTER_HPP

#include <celsus/chunkio.hpp>
#include "AnimationExporter.hpp"

extern "C"
{
  __declspec(dllexport) bool export_main(const char* filename);
}
typedef bool(*ExportMainFn)(const char*);

class ReduxExporter
{
public:
  ReduxExporter(const char* filename);
  MStatus export_all();
private:

  typedef std::string MaterialName;
  typedef std::string MeshName;
  typedef std::set<MaterialName> ExportedMaterials;
  typedef std::vector<MeshName> Meshes;

  MStatus export_hierarchy();
  MStatus export_hierarchy_inner(const MDagPath& dag_path, const std::string indent);
  MStatus export_animation();
  MStatus export_meshes();
  MStatus export_materials();
  MStatus export_cameras();
  MStatus export_camera(const MFnCamera& maya_camera);

  typedef std::map<MaterialName, Meshes> MeshesByMaterialName;
  MeshesByMaterialName meshes_by_material_name_;

  const char* filename_;
  //FILE* data_file_;
  ChunkIo writer_;
#ifdef EXPORT_JSON
  FILE* json_file_;
#else
  FILE* material_file_;
  FILE* scene_file_;
#endif

  ExportedMaterials exported_materials_;
  std::vector<MObject> materials_;
  AnimationExporter animation_exporter_;
};

#endif // #ifndef MAYA_FILE_TRANSLATOR_HPP
