/**
 * Maya Redux Exporter
 *
 * Magnus Österlind, 2009
 *
 * I've borrowed alot of code and ideas from the various other Maya exporters on the net:
 * Rob The Bloke http://nccastaff.bournemouth.ac.uk/jmacey/RobTheBloke/www/mayaapi.html
 * Bryan Ewert http://media.quilime.com/files/mel/www.ewertb.com/maya_api/How%20do%20I%20write%20a%20polygon%20mesh%20exporter_.html
 * Florian Loitsch http://florian.loitsch.com/gpExport/index-4.html#Extraction
 * Rafael Baptista http://www.gamedev.net/reference/programming/features/mayaexporter/default.asp
 * 
 * I've split the exporter into two parts, the stub and the exporter. The stub is what's loaded by Maya, and the exporter is a
 * seperate dll that's loaded from the stub's writer function. The point of this is to be able to work on the exporter while
 * Maya is running.
 *
 */ 
#include "stdafx.h"
#include "ReduxExporterStub.hpp"
#include "ScopedDeleter.hpp"

using namespace std;

#define RETURN_ON_ERROR_MSTATUS(x) \
{ MStatus status = (x); \
  if (!status) {  \
  cout << "Error: " << #x << endl;  \
  return status; \
  }\
}

#define RETURN_ON_ERROR_BOOL(x) \
{ const bool status = (x); \
  if (!status) {  \
  cout << "Error: " << #x << endl;  \
  return MS::kFailure; \
  }\
}

#define CONTINUE_ON_ERROR_MSTATUS(x) \
{ MStatus status = (x); \
  if (!status) {  \
  cout << "Error: " << #x << endl;  \
  continue; \
  }\
}

#define CONTINUE_ON_ERROR_MSG(x, msg) \
{ MStatus status = (x); \
  if (!status) {  \
  cout << "Error: " << msg << endl;  \
  continue; \
  }\
}

namespace {
  const char* kDefaultFileExtension = "rdx";
}

ReduxExporterStub::ReduxExporterStub()
{
}

ReduxExporterStub::~ReduxExporterStub()
{
}

MStatus	ReduxExporterStub::reader(const MFileObject& file, const MString& options, FileAccessMode mode)
{
  cerr << "Read method as yet unimplimented\n" << endl;
  return MS::kFailure;
}

void ReduxExporterStub::parse_options(const MString& options) {
  int i, length;
  // Start parsing.
  MStringArray optionList;
  MStringArray theOption;

  //	each option is in the form -
  //	[Option] = [Value];
  //
  //	the options are therefore split by the semi-colons
  //
  options.split(';', optionList);

  //	process each option in the string
  //
  length = optionList.length();
  for( i = 0; i < length; ++i )
  {
    theOption.clear();

    //	split the option using the '=' character. This should make 
    //	theOption[0] hold the option name, and theOption[1] hold the
    //	value of the current option
    //
    optionList[i].split( '=', theOption );
/*
    if( theOption[0] == namesonlyFlag && theOption.length() > 1 ) 
    {
      if(theOption[1].asInt()>0) 
        short_ = true;
      else 
        short_ = false;
    }
*/
  }
}

//-------------------------------------------------------------------	writer
///	\brief  Exports the scene data to a text file
///	\param  file	-	info about what to output
/// \param	options	-	a set of output options
/// \param	mode	-	all or selected
///	\return	status code
///
MStatus	ReduxExporterStub::writer(const MFileObject& file, const MString& options, FileAccessMode mode)
{
  if (options.length() > 0) {
    parse_options(options);
  }

  if( mode == MPxFileTranslator::kExportActiveAccessMode ) {
    cerr << "As yet this does not support export selected\nExporting all instead\n";
  }

  // Load the exporter dll
  HMODULE exporter_dll = LoadLibrary("plug-ins/ReduxExporter.dll");
  if (NULL == exporter_dll) {
    cerr << "[ERROR] Could not load ReduxExporter.dll" << endl;
    return MS::kFailure;
  }
  SCOPED_DELETER(&FreeLibrary, exporter_dll);

  // Find the exporter function
  typedef bool(*ExportMainFn)(const char*);
  ExportMainFn export_main = reinterpret_cast<ExportMainFn>(GetProcAddress(exporter_dll, "export_main"));
  if (NULL == export_main) {
    cerr << "[ERROR] Unable to find export_main function" << endl;
    return MS::kFailure;
  }
/*
#pragma warning(suppress: 4996)
  data_file_ = fopen(file.fullName().asChar(), "wb");
  if (NULL == data_file_) {
    cerr << "[ERROR] Could not open file for writing " << file.fullName().asChar() << endl;
    return MS::kFailure;
  }
  SCOPED_DELETER(&fclose, data_file_);
*/
  return export_main(file.fullName().asChar()) ? MS::kSuccess : MS::kFailure;
}

MString ReduxExporterStub::defaultExtension () const {
  return kDefaultFileExtension;
}

MPxFileTranslator::MFileKind ReduxExporterStub::identifyFile( const MFileObject& fileName, const char* buffer, short size) const {
  const char* str = fileName.name().asChar();
  return strstr(str, kDefaultFileExtension) != NULL ? kCouldBeMyFileType : kNotMyFileType;
}

void* ReduxExporterStub::creator() {
  return new ReduxExporterStub;
}

#define MLL_EXPORT extern __declspec(dllexport) 

//-------------------------------------------------------------------

/// specifies a script to use for the user interface options box
char* g_OptionScript = "MayaFileExportScript";

/// a set of default options for the exporter
char* g_DefaultOptions = "-namesonly=0;";

//-------------------------------------------------------------------	initializePlugin
///	\brief	initializePlugin( MObject obj )
///	\param	obj		-	the plugin handle
///	\return	MS::kSuccess if ok
///	\note	Registers all of the new commands, file translators and new 
///			node types.
///
MLL_EXPORT MStatus initializePlugin( MObject obj )
{
  MStatus status;
  MFnPlugin plugin(obj, "Magnus Österlind", "1.0", "Any");

  // Register the translator with the system
  status =  plugin.registerFileTranslator( "ReduxExporter", "none",
    ReduxExporterStub::creator,
    (char*)g_OptionScript,
    (char*)g_DefaultOptions );  

  if (status != MS::kSuccess) {
    status.perror("MayaExportCommand::registerFileTranslator");
  }
  return status;

}

//-------------------------------------------------------------------	uninitializePlugin
///	\brief	uninitializePlugin( MObject obj )
///	\param	obj	-	the plugin handle to un-register
///	\return	MS::kSuccess if ok
///	\note	un-registers the plugin and destroys itself
///
MLL_EXPORT MStatus uninitializePlugin( MObject obj )
{
  MFnPlugin plugin( obj );
  MStatus status =  plugin.deregisterFileTranslator( "ReduxExporter" );
  if (status != MS::kSuccess) {
    status.perror("MayaExportCommand::deregisterFileTranslator");
  }
  return status;
}
