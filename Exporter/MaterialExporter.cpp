#include "stdafx.h"
#include "MaterialExporter.hpp"
#include "ExporterUtils.hpp"

namespace fs = boost::filesystem;

void write_color_py(FILE* file, const MColor& value, const std::string& setting_name, const std::string& material_name) 
{
  fprintf(file, "\t\t\t(\"%s\", dx.Color(%f, %f, %f, %f)),\n", 
    setting_name.c_str(), value[0], value[1], value[2], value[3]);
}

void write_float1_py(FILE* file, const float value, const std::string& setting_name, const std::string& material_name) 
{
  fprintf(file, "\t\t\t(\"%s\", %f),\n", setting_name.c_str(), value);
}

void write_float4_py(FILE* file, const MColor& value, const std::string& setting_name, const std::string& material_name) 
{
  fprintf(file, "\t\t\t(\"%s\", dx.Vector4(%f, %f, %f, 1)),\n", setting_name.c_str(), value[0], value[1], value[2]);
}

void write_color_json(FILE* file, const MColor& value, const std::string& setting_name, const bool comma = false) 
{
  fprintf(file, "\t\t\t{ \"name\" : \"%s\", \"type\" : \"color\", \"value\" : [%f, %f, %f, %f] }%s\n", 
    setting_name.c_str(), value[0], value[1], value[2], value[3], comma ? "," : "" );
}

void write_float1_json(FILE* file, const float value, const std::string& setting_name, const bool comma = false) 
{
  fprintf(file, "\t\t\t{ \"name\" : \"%s\", \"type\" : \"float\", \"value\" : %f }%s\n", 
    setting_name.c_str(), value, comma ? "," : "");
}

void write_float4_json(FILE* file, const MColor& value, const std::string& setting_name, const bool comma = false) 
{
  fprintf(file, "\t\t\t{ \"name\" : \"%s\", \"type\" : \"vec4\", \"value\" : [%f, %f, %f, 1] }%s\n", 
    setting_name.c_str(), value[0], value[1], value[2], comma ? "," : "");
}

float color_to_grayscale(const MColor& value) 
{
  return value[0] * 0.3f + value[1] * 0.59f + value[2] * 0.11f;
}

template<class Shader>
std::string find_texture(const Shader& shader) 
{
  MPlug color_plug(shader.findPlug("color"));

  // get plugs connected to colour attribute
  MPlugArray plugs;
  color_plug.connectedTo(plugs,true,false);

  // see if any file textures are present
  for( uint32_t i = 0; i != plugs.length(); ++i) {
    // if file texture found
    if(plugs[i].node().apiType() == MFn::kFileTexture) {
      // bind a function set to it ....
      MFnDependencyNode fnDep(plugs[i].node());

      // get the attribute for the full texture path
      MStatus status;
      MPlug ftn = fnDep.findPlug("ftn", &status);
      CONTINUE_ON_ERROR_MSTATUS(status);

      // get the filename from the attribute
      MString filename;
      ftn.getValue(filename);

      return std::string(filename.asChar());
    }
  } 
  return std::string();
}

template<class Shader> 
void write_common_material_props(FILE* material_file, const Shader& shader, const bool write_comma) 
{
  const std::string material_name(sanitize_name(shader.name().asChar()));
  const std::string kTransparencyName("transparency");
  const std::string kAmbientName("ambient_color");
  const std::string kDiffuseName("diffuse_color");
  const std::string kEmissiveName("emissive_color");
  const std::string texture_filename(find_texture(shader));
  const float trans = color_to_grayscale(shader.transparency());
  const char* psz_name = material_name.c_str();

#ifdef EXPORT_JSON
  fprintf(material_file, "\t\t{ \"name\" : \"%s\",\n", material_name.c_str());
  fprintf(material_file, "\t\t\"values\" : [\n");
  write_float1_json(material_file, trans, kTransparencyName, true);
  write_color_json(material_file, shader.ambientColor(), kAmbientName, true);
  write_color_json(material_file, shader.color(), kDiffuseName, true);
  write_color_json(material_file, shader.incandescence(), kEmissiveName, false);
  fprintf(material_file, "\t\t] }%s\n", (write_comma ? "," : ""));
#else

  fprintf(material_file, "class %s():\n", psz_name);
  fprintf(material_file, "\tdef __init__(self): \n\t\tself.name = \"%s\"\n", psz_name);

  fprintf(material_file, "\t\tself.values = [\n");
  write_float1_py(material_file, trans, kTransparencyName, material_name);
  write_color_py(material_file, shader.ambientColor(), kAmbientName, material_name);
  write_color_py(material_file, shader.color(), kDiffuseName, material_name);
  write_color_py(material_file, shader.incandescence(), kEmissiveName, material_name);

  if (!texture_filename.empty()) {
    fs::path texture_path(texture_filename);
    fprintf(material_file, "\t\t\t(\"diffuse_texture\", \"%s\"),\n", texture_path.filename().c_str());
  }

  fprintf(material_file, "\t\t]");
  fprintf(material_file, "\n");
#endif
}


bool material_has_texture(MObject material) 
{
  if (!material.hasFn(MFn::kLambert)) {
    return false;
  }
  MFnLambertShader lambert(material);
  MPlug color_plug(lambert.findPlug("color"));

  // get plugs connected to colour attribute
  MPlugArray plugs;
  color_plug.connectedTo(plugs,true,false);

  // see if any file textures are present
  for( uint32_t i = 0; i != plugs.length(); ++i) {
    // if file texture found
    if(plugs[i].node().apiType() == MFn::kFileTexture) {
      // bind a function set to it ....
      MFnDependencyNode fnDep(plugs[i].node());

      // get the attribute for the full texture path
      MStatus status;
      MPlug ftn = fnDep.findPlug("ftn", &status);
      CONTINUE_ON_ERROR_MSTATUS(status);
      return true;
    }
  } 
  return false;
}


std::string make_shader_name(const MObject& src_node) 
{
  const std::string texture_suffix(material_has_texture(src_node) ? "Texture" : "");
  std::string shader_base("Diffuse");
  if (src_node.hasFn(MFn::kLambert)) {
    shader_base = "Diffuse";
  } else if (src_node.hasFn(MFn::kBlinn)) {
    shader_base = "Specular";
  } else if (src_node.hasFn(MFn::kPhong)) {
    shader_base = "Specular";
  }
  return sanitize_name(shader_base + texture_suffix);
}

MObject get_surface_shader(MObject shader_set)
{
  MFnDependencyNode dnset(shader_set);
  MObject ssattr = dnset.attribute( MString( "surfaceShader" ) );
  MPlug ssplug( shader_set, ssattr );
  MPlugArray srcplugarray;
  ssplug.connectedTo( srcplugarray, true, false );
  return srcplugarray[0].node();
}

MStatus write_material(FILE* file, MObject shader_node, const bool write_comma) 
{
  // We're interested in the actual api type, not the function sets, so we use a switch/case
  switch(shader_node.apiType()) {
    case MFn::kLambert:
      {
        MFnLambertShader shader(shader_node);
        write_common_material_props(file, shader, write_comma);
        return MS::kSuccess;
      }

    case MFn::kBlinn:
      {
        MFnBlinnShader shader(shader_node);
        write_common_material_props(file, shader, write_comma);
        return MS::kSuccess;
      }

    case MFn::kPhong:
      {
        MFnPhongShader shader(shader_node);
        write_common_material_props(file, shader, write_comma);
        return MS::kSuccess;
      }
  }

  return MS::kFailure;
}

