#include "stdafx.h"
#include "ExporterUtils.hpp"

std::string sanitize_name(const std::string& input) 
{
  std::string output(input);
  const char invalid_tokens[] = ":/ ";
  const char valid_token = '_';
  for (size_t i = 0; i < input.length(); ++i) {
    if (strchr(invalid_tokens, input[i])) {
      output[i] = valid_token;
    }
  }
  return output;
}


std::string make_material_name(const MObject& src_node) {
  std::string candidate("Unknown");
  if (src_node.hasFn(MFn::kLambert)) {
    MFnLambertShader shader(src_node);
    candidate = shader.name().asChar();
  } else if (src_node.hasFn(MFn::kBlinn)) {
    MFnBlinnShader shader(src_node);
    candidate = shader.name().asChar();
  } else if (src_node.hasFn(MFn::kPhong)) {
    MFnPhongShader shader(src_node);
    candidate = shader.name().asChar();
  }
  return sanitize_name(candidate);
}

void decompose_matrix(D3DXVECTOR3& d3d_pos, D3DXQUATERNION& d3d_rot, D3DXVECTOR3& d3d_scale, const MMatrix& mtx)
{
  MTransformationMatrix trans_mtx(mtx);
  MVector translation = trans_mtx.getTranslation(MSpace::kPostTransform);

  double q_x, q_y, q_z, q_w;
  trans_mtx.getRotationQuaternion(q_x, q_y, q_z, q_w, MSpace::kPostTransform);

  double scale[3];
  trans_mtx.getScale(scale, MSpace::kPostTransform);

  d3d_pos = D3DXVECTOR3((float)translation.x, (float)translation.y, (float)-translation.z);
  d3d_rot = D3DXQUATERNION ((float)q_x, (float)q_y, (float)-q_z, (float)q_w);
  d3d_scale = D3DXVECTOR3((float)scale[0], (float)scale[1], (float)scale[2]);
}


D3DXMATRIX to_matrix(const MMatrix& mtx)
{
  MTransformationMatrix trans_mtx(mtx);
  MVector translation = trans_mtx.getTranslation(MSpace::kPostTransform);

  double q_x, q_y, q_z, q_w;
  trans_mtx.getRotationQuaternion(q_x, q_y, q_z, q_w);

  double scale[3];
  trans_mtx.getScale(scale, MSpace::kPostTransform);

  D3DXVECTOR3 d3d_pos((float)translation.x, (float)translation.y, (float)-translation.z);
  D3DXVECTOR3 d3d_scale((float)scale[0], (float)scale[1], (float)-scale[2]);
  D3DXQUATERNION d3d_rot((float)q_x, (float)q_y, (float)-q_z, (float)q_w);
  D3DXVECTOR3 kvtxZero(0,0,0);
  D3DXQUATERNION qtId;
  D3DXQuaternionIdentity(&qtId);

  D3DXMATRIX d3d_mtx;
  D3DXMatrixTransformation(&d3d_mtx, &kvtxZero, &qtId, &d3d_scale, &kvtxZero, &d3d_rot, &d3d_pos);
  return d3d_mtx;
}

D3DXVECTOR3 to_vector3(const MPoint& pt) 
{
  return D3DXVECTOR3(static_cast<float>(pt.x), static_cast<float>(pt.y), static_cast<float>(-1.0f * pt.z));
}

D3DXVECTOR3 to_vector3(const MVector& pt) 
{
  return D3DXVECTOR3(static_cast<float>(pt.x), static_cast<float>(pt.y), static_cast<float>(-1.0f * pt.z));
}

D3DXVECTOR3 to_vector3(const MFloatVector& pt) 
{
  return D3DXVECTOR3(static_cast<float>(pt[0]), static_cast<float>(pt[1]), static_cast<float>(-1.0f * pt[2]));
}

std::string strip_pipes(const std::string& str)
{
  if (str.length() < 2) {
    return str;
  }

  const uint32_t start_idx = str[0] == '|' ? 1 : 0;
  const uint32_t len = (uint32_t)str.length();
  const uint32_t count = str[len - 1] == '|' ? len - 1 : len;
  return str.substr(start_idx, count);
}
