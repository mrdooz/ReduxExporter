#ifndef EXPORTER_UTILS_HPP
#define EXPORTER_UTILS_HPP

#define RETURN_ON_ERROR_MSTATUS(x) \
{ MStatus status = (x); \
  if (!status) {  \
  std::cout << "Error: " << #x << std::endl;  \
  std::cout << status.errorString() << std::endl; \
  return status; \
  }\
}

#define RETURN_ON_ERROR_MSTATUS_FN(x, fn) \
{ MStatus status = (x); \
  if (!status) {  \
  std::cout << "Error: " << #fn << std::endl;  \
  std::cout << status.errorString() << std::endl; \
  return status; \
  }\
}

#define RETURN_ON_ERROR_BOOL(x) \
{ const bool status = (x); \
  if (!status) {  \
  std::cout << "Error: " << #x << std::endl;  \
  return MS::kFailure; \
  }\
}

#define CONTINUE_ON_ERROR_MSTATUS(x) \
{ MStatus status = (x); \
  if (!status) {  \
  std::cout << "Error: " << #x << std::endl;  \
  std::cout << status.errorString() << std::endl; \
  continue; \
  }\
}

#define CONTINUE_ON_ERROR_MSG(x, msg) \
{ MStatus status = (x); \
  if (!status) {  \
  std::cout << "Error: " << msg << std::endl;  \
  std::cout << status.errorString() << std::endl; \
  continue; \
  }\
}

std::string sanitize_name(const std::string& input);
std::string make_material_name(const MObject& src_node);

template<typename T>
bool write_generic(FILE* file, const T value) 
{
  return fwrite(&value, sizeof(T), 1, file) == 1;
}


inline bool write_int(FILE* file, const int value) 
{
  return fwrite(&value, sizeof(value), 1, file) == 1;
}

// Strings are written as [len, data]
inline bool write_string(FILE* file, const std::string& str) 
{
  const int32_t len = (int32_t)str.length();
  if (!write_int(file, len) ) {
    return false;
  }
  return fwrite(&str.c_str()[0], 1, len, file) == len;
}

inline bool write_raw_data(FILE* file, const uint8_t* data, const uint32_t len) 
{
  return fwrite(data, 1, len, file) == len;
}


void decompose_matrix(D3DXVECTOR3& d3d_pos, D3DXQUATERNION& d3d_rot, D3DXVECTOR3& d3d_scale, const MMatrix& mtx);
D3DXMATRIX to_matrix(const MMatrix& mtx);
D3DXVECTOR3 to_vector3(const MPoint& pt);
D3DXVECTOR3 to_vector3(const MVector& pt);
D3DXVECTOR3 to_vector3(const MFloatVector& pt);

std::string strip_pipes(const std::string& str);

#endif
