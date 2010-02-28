/**
 * Maya Redux Exporter
 */ 
#include "stdafx.h"
#include "ReduxExporter.hpp"
#include "ScopedDeleter.hpp"
#include "ExporterUtils.hpp"
#include "MeshExporter.hpp"
#include "AnimationExporter.hpp"
#include "MaterialExporter.hpp"

using namespace std;
namespace fs = boost::filesystem;


namespace {
  const char* kDefaultFileExtension = "rdx";
}

ReduxExporter::ReduxExporter(const char* filename) 
  : filename_(filename)
  , writer_()
  , animation_exporter_(writer_)
#ifdef EXPORT_JSON
  , json_file_(NULL)
#else
  , material_file_(NULL)
  , scene_file_(NULL)
#endif
{
  writer_.init_writer(ChunkIo::MainHeader::CompressedZLib);
}

MStatus ReduxExporter::export_all() 
{
  char time_buf[128];
  _strtime_s(time_buf, sizeof(time_buf));
  cout << "***************************************** STARTING EXPORT (" << time_buf << ")" << endl;
  fs::path out_path(filename_);
  out_path.replace_extension();


#ifdef EXPORT_JSON
  const string json_filename(out_path.string() + ".json");
  RETURN_ON_ERROR_BOOL(fopen_s(&json_file_, json_filename.c_str(), "wt") == 0);
  SCOPED_DELETER(&fclose, json_file_);
  fprintf(json_file_, "{\n");
#else
  const string materials_filename(out_path.string() + "_materials.py");
  const string scene_filename(out_path.string() + "_scene.py");
  RETURN_ON_ERROR_BOOL(fopen_s(&material_file_, materials_filename.c_str(), "wt") == 0);
  fprintf(material_file_, "import dx_ext as dx\n\n");
  RETURN_ON_ERROR_BOOL(fopen_s(&scene_file_, scene_filename.c_str(), "wt") == 0);

  SCOPED_DELETER(&fclose, material_file_);
  SCOPED_DELETER(&fclose, scene_file_);
#endif

  RETURN_ON_ERROR_MSTATUS(export_hierarchy());

  RETURN_ON_ERROR_MSTATUS(export_animation());

  RETURN_ON_ERROR_MSTATUS(export_meshes());

  RETURN_ON_ERROR_MSTATUS(export_cameras());

  RETURN_ON_ERROR_MSTATUS(export_materials());

  writer_.end_of_data();

  const string out_filename(out_path.string() + ".rdx");
  uint8_t* buf = NULL;
  uint32_t len = 0;
  writer_.get_buffer(buf, len);
  RETURN_ON_ERROR_BOOL(write_file(buf, len, out_filename.c_str()));

#ifdef EXPORT_JSON
  fprintf(json_file_, "\n}");
#endif

  _strtime_s(time_buf, sizeof(time_buf));
  cout << "***************************************** EXPORTING SUCESSFULLY DONE (" << time_buf << ")" << endl;

  return MS::kSuccess;
}

MStatus ReduxExporter::export_materials()
{
#ifdef EXPORT_JSON
  // Export materials
  fprintf(json_file_, "\t\"materials\" : [\n" );
  for (uint32_t i = 0; i < materials_.size(); ++i) {
    write_material(json_file_, materials_[i], i != materials_.size() - 1);
  }
  fprintf(json_file_, "\t], \n\n" );

  // Export material connections
  fprintf(json_file_, "\t\"material_connections\" : [\n" );
  uint32_t counter = 0;
  for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
    for (Meshes::iterator mesh_it = it->second.begin(); mesh_it != it->second.end(); ++mesh_it) {
      counter++;
    }
  }

  for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
    for (Meshes::iterator mesh_it = it->second.begin(); mesh_it != it->second.end(); ++mesh_it) {
      fprintf(json_file_, "\t{\"mesh\" : \"%s\", \"material\" : \"%s\"}%s\n", 
        mesh_it->c_str(), it->first.c_str(), ((--counter != 0) ? "," : ""));
    }
  }

  fprintf(json_file_, "\t], \n\n");

  // Export shader connections
  const char* psz_default_shader = "blinn_effect";
  fprintf(json_file_, "\"effect_connections\" : [ { \"effect\" : \"%s\" , \"materials\" : [\n\t", psz_default_shader);
  counter = (int32_t)meshes_by_material_name_.size();
  for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
    fprintf(json_file_, "\"%s\"%s \n\t", (*it).first.c_str(), (--counter != 0 ? "," : "") );
  }
  fprintf(json_file_, "] } ]\n");

#else
  for (uint32_t i = 0; i < materials_.size(); ++i) {
    write_material(material_file_, materials_[i]);
  }

  fprintf(scene_file_, "material_connections = [\n");

  const bool use_old_method = false;
  if (use_old_method) {
    // [(material_name, [meshes])]
    for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
      fprintf(scene_file_, "\t(\"%s\", [", it->first.c_str());
      for (Meshes::iterator mesh_it = it->second.begin(); mesh_it != it->second.end(); ++mesh_it) {
        fprintf(scene_file_, "\"%s\",", mesh_it->c_str());
      }
      fprintf(scene_file_, "]),\n");
    }
  } else {
    // [(mesh_name, material_name)]
    for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
      for (Meshes::iterator mesh_it = it->second.begin(); mesh_it != it->second.end(); ++mesh_it) {
        fprintf(scene_file_, "\t(\"%s\",\"%s\"),\n", mesh_it->c_str(), it->first.c_str());
      }
    }
  }
  fprintf(scene_file_, "]\n");

  // export shader connections
  const char* psz_default_shader = "blinn_effect";
  fprintf(scene_file_, "effect_connections = [(\"%s\", [\n\t", psz_default_shader);
  for (MeshesByMaterialName::iterator it = meshes_by_material_name_.begin(); it != meshes_by_material_name_.end(); ++it) {
    fprintf(scene_file_, "\"%s\", \n\t", (*it).first.c_str());
  }
  fprintf(scene_file_, "])]\n");

#endif

  return MS::kSuccess;
}

MStatus ReduxExporter::export_camera(const MFnCamera& maya_camera)
{
  const std::string camera_name(maya_camera.name().asChar());
  const MSpace::Space space = MSpace::kObject;
  const MPoint eye_pos(maya_camera.eyePoint(space));
  const MVector view_dir(maya_camera.viewDirection(space));
  const MVector up_dir(maya_camera.upDirection(space));
  const MVector right_dir(maya_camera.rightDirection(space));
  const float aspect_ratio = (float)maya_camera.aspectRatio();
  const float horizontal_fov = (float)maya_camera.horizontalFieldOfView();
  const float vertical_fov = (float)maya_camera.verticalFieldOfView();
  const float near_plane = (float)maya_camera.nearClippingPlane();
  const float far_plane = (float)maya_camera.farClippingPlane();

  SCOPED_CHUNK(writer_, ChunkHeader::Camera);
  RETURN_ON_ERROR_BOOL(writer_.write_string(camera_name));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(to_vector3(eye_pos)));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(to_vector3(view_dir)));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(to_vector3(up_dir)));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(to_vector3(right_dir)));

  RETURN_ON_ERROR_BOOL(writer_.write_generic(aspect_ratio));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(horizontal_fov));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(vertical_fov));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(near_plane));
  RETURN_ON_ERROR_BOOL(writer_.write_generic(far_plane));

  return MS::kSuccess;
}


MStatus ReduxExporter::export_animation()
{
  return animation_exporter_.do_export();
}

MStatus ReduxExporter::export_hierarchy_inner(const MDagPath& dag_path, const string indent)
{
  cout << indent << dag_path.fullPathName() << " (" << dag_path.node().apiTypeStr() << ")" << endl;
  const uint32_t child_count = dag_path.childCount();

  writer_.write_string(strip_pipes(dag_path.fullPathName().asChar()));
  writer_.write_generic<uint32_t>(child_count);
  for (uint32_t i = 0; i < child_count; ++i) {
    MObject child = dag_path.child(i);
    MDagPath child_path;
    MDagPath::getAPathTo(child, child_path);
    export_hierarchy_inner(child_path, indent + "  ");
  }
  return MS::kSuccess;
}

MStatus ReduxExporter::export_hierarchy()
{
  SCOPED_CHUNK(writer_, ChunkHeader::Hierarchy);

  MItDag it_root;
  MDagPath root_path;
  it_root.getPath(root_path);
  const uint32_t child_count = root_path.childCount();
  writer_.write_string("root");
  writer_.write_generic<uint32_t>(child_count);
  for (uint32_t i = 0; i < child_count; ++i) {
    MObject child = root_path.child(i);
    MDagPath child_path;
    MDagPath::getAPathTo(child, child_path);
    RETURN_ON_ERROR_MSTATUS(export_hierarchy_inner(child_path, ""));
  }
  return MS::kSuccess;
}


MStatus ReduxExporter::export_meshes()
{
  MeshExporter mesh_exporter(meshes_by_material_name_, exported_materials_, materials_, writer_, animation_exporter_);

  MStatus status;
  for( MItDag it(MItDag::kDepthFirst, MFn::kMesh); !it.isDone(); it.next() ) {

    MDagPath dag_path;
    CONTINUE_ON_ERROR_MSTATUS(it.getPath(dag_path));

    MFnMesh maya_mesh(dag_path, &status);
    CONTINUE_ON_ERROR_MSG(status, "Error creating MFnMesh from path");

    CONTINUE_ON_ERROR_MSTATUS(mesh_exporter.export_mesh(maya_mesh, dag_path));
  }
  return MS::kSuccess;
}

MStatus ReduxExporter::export_cameras()
{
  for( MItDag it(MItDag::kDepthFirst, MFn::kCamera); !it.isDone(); it.next() ) {

    MDagPath dag_path;
    CONTINUE_ON_ERROR_MSTATUS(it.getPath(dag_path));

    MFnCamera maya_camera(dag_path);
    CONTINUE_ON_ERROR_MSTATUS(export_camera(maya_camera));
  }
  return MS::kSuccess;
}



extern "C"
{
  bool export_main(const char* filename)
  {
    ReduxExporter exporter(filename);
    return exporter.export_all() == MS::kSuccess;
  }
}
