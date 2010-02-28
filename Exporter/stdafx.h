// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <tchar.h>

#include <fstream>
#include <iostream>

#include <stdint.h>
#include <windows.h>

#include <maya/MAnimUtil.h>
#include <maya/MAnimControl.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFileIO.h>
#include <maya/MFileObject.h>
#include <maya/MFn.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnCamera.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnBlinnShader.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#define MLIBRARY_DONTUSE_MFC_MANIFEST
#include <maya/MLibrary.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MStringArray.h>
#include <maya/MUint64Array.h>
#include <maya/MFnSubd.h>
#include <maya/MfnDagNode.h>
#include <maya/MFnTransform.h>

#include <boost/filesystem.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <D3DX10.h>

#include <boost/shared_ptr.hpp>
#include <celsus/ChunkIO.hpp>
#include <celsus/celsus.hpp>
#include <celsus/CelsusExtra.hpp>