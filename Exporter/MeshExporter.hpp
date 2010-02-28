#ifndef MESH_EXPORTER_HPP
#define MESH_EXPORTER_HPP


struct SuperVertex
{
  SuperVertex(const D3DXVECTOR3& pos, const D3DXVECTOR3& normal, const D3DXVECTOR2& uv) 
    : pos_(pos), normal_(normal), uv_(uv) 
  {
  }

  SuperVertex(const D3DXVECTOR3& pos, const D3DXVECTOR3& normal) 
    : pos_(pos), normal_(normal), uv_(0,0) 
  {
  }

  SuperVertex() 
    : pos_(0,0,0), normal_(0,0,0), uv_(0,0) 
  {
  }

  D3DXVECTOR3 pos_;
  D3DXVECTOR3 normal_;
  D3DXVECTOR2 uv_;
};


typedef std::vector<SuperVertex> SuperVerts;

typedef std::vector<D3DXVECTOR2> UVs;
struct Influence 
{
  Influence(const uint32_t bone_index, const float weight) : bone_index(bone_index), weight(weight) {}
  uint32_t  bone_index;
  float     weight;
};

typedef std::vector<Influence> Influences;

struct SkinningData 
{
  std::vector<std::string> joint_names;
  std::vector<Influences> influences;
};

struct MeshRawData 
{
  MPointArray positions;
  MFloatVectorArray normals;
  std::vector<UVs> uvs;
  SkinningData skinning_data;
};

// A vertex contains indices into the raw data 
struct Vertex 
{
  Vertex() : position_index(-1), normal_index(-1), uv_index(-1) {}
  Vertex(const uint32_t pos, const uint32_t normal, const uint32_t uv = -1) : position_index(pos), normal_index(normal), uv_index(uv) {}
  uint32_t  position_index;
  uint32_t  normal_index;
  uint32_t  uv_index;
};


struct Triangle 
{
  uint32_t i[3];
};

typedef std::vector<Vertex> Vertices;
typedef std::vector<Triangle> Triangles;

struct SubMesh 
{
  std::string name_;
  MObject shader_;

  Vertices  vertices_;
  Triangles triangles_;
};

typedef boost::shared_ptr<SubMesh> SubMeshPtr;
typedef std::vector<SubMeshPtr> SubMeshes;
typedef std::vector<MObject> Materials;

typedef boost::shared_ptr<MItMeshPolygon> MItMeshPolygonPtr;

class AnimationExporter;

class MeshExporter
{
public:
  typedef std::string MeshName;
  typedef std::vector<MeshName> Meshes;
  typedef std::string MaterialName;
  typedef std::set<MaterialName> ExportedMaterials;
  typedef std::map<MaterialName, Meshes> MeshesByMaterialName;

  MeshExporter(MeshesByMaterialName& meshes_by_material_name, ExportedMaterials& exported_materials, Materials& materials_, 
    ChunkIo& writer, const AnimationExporter& animation_exporter);
  MStatus export_mesh(const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path);

private:
  MStatus collect_raw_data(MeshRawData& raw_data, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path, const std::string& parent_path_name);
  std::string create_unique_mesh_name(const std::string& candidate);
  MStatus write_element_desc(const bool has_uvs);
  MStatus write_vertex_data( SuperVerts& super_verts,
    const MeshRawData& raw_data, 
    const Vertices& vertices,
    const Triangles& triangles, 
    const bool opposite);

  MStatus write_geometry_info(const SuperVerts& super_verts);
  MStatus get_skinning_data(SkinningData& skinning_data, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path);
  MStatus get_uvs(std::vector<UVs>& uvs, const MFnMesh& maya_mesh);
  MStatus create_sub_meshes(SubMeshes& sub_meshes, const MFnMesh& maya_mesh, const MDagPath& mesh_dag_path);
  MStatus add_triangles(SubMeshPtr& sub_mesh, MItMeshPolygon& poly_iter, const MFnMesh& maya_mesh);
  MStatus convert_mesh_local_to_polygon_local(Triangles& polygon_local_triangles, MIntArray triangle_indices, MIntArray poly_indices);

  static std::set<std::string> mesh_names_;
  ChunkIo& writer_;

  MeshesByMaterialName& meshes_by_material_name_;
  ExportedMaterials& exported_materials_;
  Materials& materials_;
  const AnimationExporter& animation_exporter_;
};

#endif
