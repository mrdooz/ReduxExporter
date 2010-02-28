#include "stdafx.h"
#include "MeshExporter.hpp"
#include "ExporterUtils.hpp"
#include "MaterialExporter.hpp"
#include "AnimationExporter.hpp"
#include "Miniball.h"
#include "vcacheopt.h"

std::set<std::string> MeshExporter::mesh_names_;

namespace fs = boost::filesystem;

// less specialized for SuperVertex
template<> struct std::less<SuperVertex> : public std::binary_function<SuperVertex, SuperVertex, bool> {
  bool operator()(const SuperVertex& lhs, const SuperVertex& rhs) const {
    for (int i = 0; i < 3; ++i ) {
      if (lhs.pos_[i] < rhs.pos_[i] ) { return true; }
      if (rhs.pos_[i] < lhs.pos_[i] ) { return false; }
    }
    for (int i = 0; i < 3; ++i ) {
      if (lhs.normal_[i] < rhs.normal_[i] ) { return true; }
      if (rhs.normal_[i] < lhs.normal_[i] ) { return false; }
    }
    for (int i = 0; i < 2; ++i ) {
      if (lhs.uv_[i] < rhs.uv_[i] ) { return true; }
      if (rhs.uv_[i] < lhs.uv_[i] ) { return false; }
    }
    return false;
  }
};


namespace 
{
  const uint32_t kPosDataSize = 3 * sizeof(float);
  const uint32_t kNormalDataSize = 3 * sizeof(float);
  const uint32_t kTexCoordDataSize = 2 * sizeof(float);
}

MeshExporter::MeshExporter(MeshesByMaterialName& meshes_by_material_name, ExportedMaterials& exported_materials, 
                           Materials& materials, ChunkIo& writer, const AnimationExporter& animation_exporter)
                           : meshes_by_material_name_(meshes_by_material_name)
                           , exported_materials_(exported_materials)
                           , materials_(materials)
                           , writer_(writer)
                           , animation_exporter_(animation_exporter)
{
}


MStatus MeshExporter::collect_raw_data(MeshRawData& raw_data, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path, const std::string& transform_path)
{
  const bool is_animated = animation_exporter_.is_animated(transform_path);
  const MSpace::Space space = is_animated ? MSpace::kObject : MSpace::kWorld;

  RETURN_ON_ERROR_MSTATUS(maya_mesh.getPoints(raw_data.positions, space));
  RETURN_ON_ERROR_MSTATUS(maya_mesh.getNormals(raw_data.normals, space));
  RETURN_ON_ERROR_MSTATUS(get_uvs(raw_data.uvs, maya_mesh));
  RETURN_ON_ERROR_MSTATUS(get_skinning_data(raw_data.skinning_data, maya_mesh, mesh_dag_path));

  return MS::kSuccess;
}


std::string MeshExporter::create_unique_mesh_name(const std::string& candidate)
{
  std::string mesh_name_candidate(candidate);
  if (mesh_names_.find(mesh_name_candidate) != mesh_names_.end()) {
    do {
      mesh_name_candidate += "a";
    } while (mesh_names_.find(mesh_name_candidate) != mesh_names_.end());
  }
  mesh_names_.insert(mesh_name_candidate);
  return mesh_name_candidate;
}

MStatus MeshExporter::write_element_desc(const bool has_uvs) 
{
  // Write the input element desc
  const int element_desc_count = 2 + (has_uvs ? 1 : 0);
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(element_desc_count));

  RETURN_ON_ERROR_BOOL(writer_.write_string("POSITION"));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(DXGI_FORMAT_R32G32B32_FLOAT));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));

  RETURN_ON_ERROR_BOOL(writer_.write_string("NORMAL"));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(DXGI_FORMAT_R32G32B32_FLOAT));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(kPosDataSize));

  if (has_uvs) {
    RETURN_ON_ERROR_BOOL(writer_.write_string("TEXCOORD"));
    RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
    RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(DXGI_FORMAT_R32G32_FLOAT));
    RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(0));
    RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(kPosDataSize + kNormalDataSize));
  }

  return MS::kSuccess;
}

MStatus MeshExporter::write_vertex_data( SuperVerts& super_verts,
                                        const MeshRawData& raw_data, 
                                        const Vertices& vertices, 
                                        const Triangles& triangles, 
                                        const bool opposite) 
{
  // Reformat the data
  // TODO: this should be done when we collect the data in the first place..
  const float normal_mul = opposite ? -1.0f : 1.0f;
  super_verts.reserve(vertices.size());

  // map indices from the vertices array to the super_verts array, which only contains unique verts
  std::map<uint32_t, uint32_t > vertex_mapping;

  // keep a mapping of super vertex -> index in super_verts array
  std::map<SuperVertex, uint32_t, std::less<SuperVertex> > super_vertex_map;

  const uint32_t vertex_count_pre = (uint32_t)vertices.size();
  for (uint32_t i = 0; i < vertices.size(); ++i) {

    D3DXVECTOR3 normal(0,0,0);
    D3DXVECTOR3 pos(0,0,0);
    D3DXVECTOR2 uv(0,0);

    // only use valid indices
    if (vertices[i].position_index < raw_data.positions.length()) {
      pos = to_vector3(raw_data.positions[vertices[i].position_index]);
    }

    if (vertices[i].normal_index < raw_data.normals.length()) {
      normal = normal_mul * to_vector3(raw_data.normals[vertices[i].normal_index]);
    }

    if (raw_data.uvs.size() > 0 ) {
      if (vertices[i].uv_index < raw_data.uvs[0].size()) {
        uv = raw_data.uvs[0][vertices[i].uv_index];
      }
    }

    SuperVertex candidate(pos, normal, uv);
    if (super_vertex_map.find(candidate) == super_vertex_map.end()) {
      super_verts.push_back(candidate);
      const uint32_t new_idx = (uint32_t)(super_verts.size() - 1);
      super_vertex_map.insert(std::make_pair(candidate, new_idx));
      vertex_mapping[i] = new_idx;
    } else {
      vertex_mapping[i] = super_vertex_map[candidate];
    }
  }

  const uint32_t vertex_count_post = (uint32_t)super_verts.size();
  cout << "vertex count: " << vertex_count_pre << " -> " << vertex_count_post << endl;


  std::vector<uint32_t> indices;
  indices.reserve(triangles.size() * 3);
  for (uint32_t i = 0; i < triangles.size(); ++i) {
    uint32_t a, b, c;
    if (opposite) {
      a = triangles[i].i[0];
      b = triangles[i].i[1];
      c = triangles[i].i[2];
    } else {
      a = triangles[i].i[2];
      b = triangles[i].i[1];
      c = triangles[i].i[0];
    }

    indices.push_back(vertex_mapping[a]);
    indices.push_back(vertex_mapping[b]);
    indices.push_back(vertex_mapping[c]);
  }

  const int vertex_count = (int32_t)super_verts.size();
  const int index_count = (int32_t)indices.size();

  const int vertex_size = sizeof(SuperVertex);
  const int index_size = sizeof(uint32_t);

  // run the vertex cache optimizer
  int32_t* index_buffer = (int32_t*)&indices[0];
  const int32_t triangle_count = index_count / 3;
  VertexCacheOptimizer vcache;
  VertexCache cache;
  const int pre_miss_count = cache.GetCacheMissCount(index_buffer, triangle_count);
  VertexCacheOptimizer::Result res = vcache.Optimize(index_buffer, triangle_count);
  if (res != VertexCacheOptimizer::Success) {
    cout << "Error running vertex cache optimzer" << endl;
  }
  const int post_miss_count = cache.GetCacheMissCount(index_buffer, triangle_count);
  cout << "vertex miss count: " << pre_miss_count << " -> " << post_miss_count << endl;


  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(vertex_count));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(vertex_size));
  RETURN_ON_ERROR_BOOL(writer_.write_raw_data((uint8_t*)&super_verts[0], sizeof(SuperVertex) * vertex_count));

  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(index_count));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(index_size));
  RETURN_ON_ERROR_BOOL(writer_.write_raw_data((uint8_t*)&indices[0], sizeof(uint32_t) * index_count));
  return MS::kSuccess;
}

MStatus MeshExporter::export_mesh(const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path) 
{
  // We get "random" failures on the dag_node ctor, with the error code "(kSuccess): API Error Log Opened",
  // but this seems to be ok.
  MStatus status;
  MFnDagNode dag_node(mesh_dag_path, &status);
  //RETURN_ON_ERROR_MSTATUS_FN(status, dag_node(mesh_dag_path, &status));

  MObject mesh_parent = dag_node.parent(0, &status);
  //RETURN_ON_ERROR_MSTATUS_FN(status, dag_node.parent(0, &status));

  MDagPath parent_path = MDagPath::getAPathTo(mesh_parent, &status);
  //RETURN_ON_ERROR_MSTATUS_FN(status, MDagPath::getAPathTo(mesh_parent, &status));

  const std::string parent_path_name(strip_pipes(parent_path.fullPathName().asChar()));
  const std::string path_name(strip_pipes(mesh_dag_path.fullPathName().asChar()));
  MeshRawData raw_data;
  RETURN_ON_ERROR_MSTATUS(collect_raw_data(raw_data, maya_mesh, mesh_dag_path, parent_path_name));

  SubMeshes sub_meshes;
  RETURN_ON_ERROR_MSTATUS(create_sub_meshes(sub_meshes, maya_mesh, mesh_dag_path));

  bool found_triangles = false;
  for (SubMeshes::iterator it = sub_meshes.begin(); it != sub_meshes.end(); ++it) {
    SubMeshPtr& sub_mesh = *it;
    if (sub_mesh->triangles_.size() != 0) {
      found_triangles = true;
      break;
    }
  }

  if (!found_triangles) {
    std::cout << "Skipping mesh: " << mesh_dag_path.fullPathName() << " no triangles found." << std::endl;
    return MS::kSuccess;
  }

  /*
  SCOPED_CHUNK(ChunkHeader::Geometry);
  const int32_t node_id = 0;
  RETURN_ON_ERROR_BOOL(writer_.write_generic<int>(node_id));
  */

  int32_t mesh_name_iter = 0;
  for (SubMeshes::iterator it = sub_meshes.begin(); it != sub_meshes.end(); ++it) {
    SubMeshPtr& sub_mesh = *it;
    if (sub_mesh->triangles_.size() == 0) {
      continue;
    }
    SCOPED_CHUNK(writer_, ChunkHeader::Mesh);
    MObject shader = sub_mesh->shader_;

    const std::string mesh_name(create_unique_mesh_name(sanitize_name(toString("%s_%d", path_name.c_str(), mesh_name_iter++))));

    const std::string material_name(make_material_name(shader));
    const std::string shader_name(make_shader_name(shader));

    meshes_by_material_name_[material_name].push_back(mesh_name);
    if (exported_materials_.find(material_name) == exported_materials_.end()) {
      materials_.push_back(shader);
      //write_material(material_file_, shader);
      exported_materials_.insert(material_name);
    }

    RETURN_ON_ERROR_BOOL(writer_.write_string(mesh_name));
    RETURN_ON_ERROR_BOOL(writer_.write_string(parent_path_name));

    //const bool has_texture = material_has_texture(shader);
    // We always save a desc containing texture coords
    const bool has_texture = true;
    RETURN_ON_ERROR_MSTATUS(write_element_desc(has_texture));

    bool opposite = false;
    maya_mesh.findPlug("opposite",true).getValue(opposite);
    std::vector<SuperVertex> super_verts;
    RETURN_ON_ERROR_MSTATUS(write_vertex_data(super_verts, raw_data, sub_mesh->vertices_, sub_mesh->triangles_, opposite));
    RETURN_ON_ERROR_MSTATUS(write_geometry_info(super_verts));
    mesh_name_iter++;
  }
  return MS::kSuccess;
}

MStatus MeshExporter::write_geometry_info(const SuperVerts& super_verts)
{

  miniball::Miniball<3> mb;
  for (size_t i = 0; i < super_verts.size(); ++i) {
    mb.check_in(miniball::Point<3>(super_verts[i].pos_[0], super_verts[i].pos_[1], super_verts[i].pos_[2]));
  }

  mb.build();

  const miniball::Point<3> center = mb.center();
  const double radius = sqrt(mb.squared_radius());

  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)center[0]));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)center[1]));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)center[2]));
  RETURN_ON_ERROR_BOOL(writer_.write_generic<float>((float)radius));

  return MS::kSuccess;
}

MStatus MeshExporter::convert_mesh_local_to_polygon_local(
  std::vector<Triangle>& polygon_local_triangles, MIntArray triangle_indices, MIntArray poly_indices) 
{
  uint32_t i = 0;
  while (i < triangle_indices.length()) {
    Triangle currentTriangle;
    for (uint32_t j = 0; j < 3; ++j) {
      // search for the triangle index in the poly index
      const int32_t cur_index = triangle_indices[i];
      bool found = false;
      for (uint32_t k = 0; k < poly_indices.length() && !found; ++k) {
        if (poly_indices[k] == cur_index) {
          currentTriangle.i[j] = k;
          found = true;
        }
      }
      if (!found) {
        return MS::kFailure;
      }
      i++;
    }
    polygon_local_triangles.push_back(currentTriangle);
  }
  return MS::kSuccess;
}

MStatus MeshExporter::add_triangles(SubMeshPtr& sub_mesh, MItMeshPolygon& poly_iter, const MFnMesh& maya_mesh) 
{
  // Calc the vertex indices
  MPointArray triangle_points;
  MIntArray mesh_local_triangle_indices;
  MIntArray mesh_local_polygon_indices;
  poly_iter.getTriangles(triangle_points, mesh_local_triangle_indices);
  poly_iter.getVertices(mesh_local_polygon_indices);

  std::vector<Triangle> poly_local_triangles;
  convert_mesh_local_to_polygon_local(poly_local_triangles, mesh_local_triangle_indices, mesh_local_polygon_indices);

  // make the indices relative to current vertex list
  const uint32_t vertex_offset = (uint32_t)sub_mesh->vertices_.size();
  for (uint32_t i = 0; i < poly_local_triangles.size(); ++i) {
    poly_local_triangles[i].i[0] += vertex_offset;
    poly_local_triangles[i].i[1] += vertex_offset;
    poly_local_triangles[i].i[2] += vertex_offset;

    sub_mesh->triangles_.push_back(poly_local_triangles[i]);
  }

  MStringArray uv_set_names;
  const bool uvs_available = MS::kSuccess == maya_mesh.getUVSetNames(uv_set_names);

  // Create the vertices
  for (uint32_t i = 0; i < poly_iter.polygonVertexCount(); ++i) {
    int32_t uv_index = -1;
    if (uvs_available) {
      poly_iter.getUVIndex(i, uv_index, &uv_set_names[0]);
    }
    sub_mesh->vertices_.push_back(Vertex(poly_iter.vertexIndex(i), poly_iter.normalIndex(i), uv_index));
  }
  return MS::kSuccess;
}

MStatus MeshExporter::create_sub_meshes(SubMeshes& sub_meshes, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path)
{
  MObjectArray shaders;
  MIntArray shader_indices;
  RETURN_ON_ERROR_MSTATUS(maya_mesh.getConnectedShaders(mesh_dag_path.instanceNumber(), shaders, shader_indices));
  if (shaders.length() == 0) {
    return MS::kSuccess;
  }

  for (uint32_t i = 0; i < shaders.length(); ++i) {
    SubMesh* sub_mesh = new SubMesh();
    sub_mesh->shader_ = get_surface_shader(shaders[i]);
    sub_meshes.push_back(SubMeshPtr(sub_mesh));
  }

  MStatus status;
  MItMeshPolygon face_iter(maya_mesh.object(),&status);
  RETURN_ON_ERROR_MSTATUS(status);
  for (; !face_iter.isDone(); face_iter.next()) {
    const uint32_t face_index = face_iter.index();
    RETURN_ON_ERROR_MSTATUS(add_triangles(sub_meshes[shader_indices[face_index]], face_iter, maya_mesh));
  }

  return MS::kSuccess;
}

MStatus MeshExporter::get_uvs(std::vector<UVs>& uvs, const MFnMesh& maya_mesh) 
{
  MStringArray uv_set_names;
  RETURN_ON_ERROR_MSTATUS(maya_mesh.getUVSetNames(uv_set_names));

  for (uint32_t i = 0; i < uv_set_names.length(); ++i) {
    MFloatArray us;
    MFloatArray vs;
    RETURN_ON_ERROR_MSTATUS(maya_mesh.getUVs(us, vs, &uv_set_names[i]));

    UVs cur_uvs;
    cur_uvs.reserve(us.length());
    for (uint32_t j = 0; j < us.length(); ++j) {
      cur_uvs.push_back(D3DXVECTOR2(us[j], 1.0f - vs[j]));
    }
    uvs.push_back(cur_uvs);
  }
  return MS::kSuccess;
}

MStatus MeshExporter::get_skinning_data(SkinningData& skinning_data, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path) 
{
  for (MItDependencyNodes skin_cluster_it(MFn::kSkinClusterFilter); !skin_cluster_it.isDone(); skin_cluster_it.next())
  {
    MStatus status = MS::kSuccess;
    MObject depNodeObject = skin_cluster_it.item();
    MFnSkinCluster maya_skin_cluster(depNodeObject, &status);
    CONTINUE_ON_ERROR_MSG(status, "Error creating skin cluster");

    // Check if our mesh is the output for this skin cluster
    const uint32_t shape_index = maya_skin_cluster.indexForOutputShape(mesh_dag_path.node(), &status);
    if (!status) {
      continue;
    }

    // Get the influence objects (joints)
    MDagPathArray influence_objects;
    maya_skin_cluster.influenceObjects(influence_objects, &status);
    CONTINUE_ON_ERROR_MSG(status, "Error getting influence objects");
    for (uint32_t i = 0; i < influence_objects.length(); ++i) {
      skinning_data.joint_names.push_back(influence_objects[i].partialPathName().asChar());
    }

    // Get the influences
    MObject maya_input_object = maya_skin_cluster.inputShapeAtIndex(shape_index, &status);
    skinning_data.influences.resize(maya_mesh.numVertices());

    // iterate over all points (= components in Maya) and get the influences (= [boneIndex, weight])
    uint32_t pointCounter = 0;
    for (MItGeometry geometryIt(maya_input_object); !geometryIt.isDone(); geometryIt.next(), ++pointCounter) {
      // Get weights
      MFloatArray mayaWeightArray;
      uint32_t numInfluences;
      maya_skin_cluster.getWeights(mesh_dag_path, geometryIt.component(), mayaWeightArray, numInfluences);

      // Store them for this point
      for (uint32_t j = 0; j < mayaWeightArray.length(); ++j) {
        if (mayaWeightArray[j] == 0) {
          continue;
        }
        skinning_data.influences[pointCounter].push_back(Influence(j, mayaWeightArray[j]));
      }
    }
  }
  return MS::kSuccess;
}
