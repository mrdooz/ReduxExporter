#include "stdafx.h"
#include "ReduxExporter.hpp"

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

	std::cout << "A Simple File translator plugin example. Rob Bateman 2004 [robthebloke@hotmail.com]" << std::endl;
	MFnPlugin plugin( obj, "Rob Bateman", "3.0", "Any");

	// Register the translator with the system
	status =  plugin.registerFileTranslator( "SimpleExport", "none",
		ReduxExporter::creator,
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
	MStatus status =  plugin.deregisterFileTranslator( "SimpleExport" );
	if (status != MS::kSuccess) {
		status.perror("MayaExportCommand::deregisterFileTranslator");
	}
	return status;
}
