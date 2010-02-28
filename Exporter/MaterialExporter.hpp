#ifndef MATERIAL_EXPORTER_HPP
#define MATERIAL_EXPORTER_HPP

MObject get_surface_shader(MObject shader_set);
std::string make_shader_name(const MObject& src_node);
MStatus write_material(FILE* file, MObject shader_node, const bool write_comma);

#endif
