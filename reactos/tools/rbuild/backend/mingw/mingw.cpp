
#include "../../pch.h"

#include "mingw.h"
#include <assert.h>

using std::string;
using std::vector;

static class MingwFactory : public Backend::Factory
{
public:
	MingwFactory() : Factory ( "mingw" ) {}
	Backend* operator() ( Project& project )
	{
		return new MingwBackend ( project );
	}
} factory;


MingwBackend::MingwBackend ( Project& project )
	: Backend ( project )
{
}

void
MingwBackend::Process ()
{
	DetectPCHSupport();

	CreateMakefile ();
	GenerateHeader ();
	GenerateGlobalVariables ();
	GenerateAllTarget ();
	GenerateInitTarget ();
	GenerateXmlBuildFilesMacro();
	for ( size_t i = 0; i < ProjectNode.modules.size (); i++ )
	{
		Module& module = *ProjectNode.modules[i];
		ProcessModule ( module );
	}
	CheckAutomaticDependencies ();
	CloseMakefile ();
}

void
MingwBackend::CreateMakefile ()
{
	fMakefile = fopen ( ProjectNode.makefile.c_str (), "w" );
	if ( !fMakefile )
		throw AccessDeniedException ( ProjectNode.makefile );
	MingwModuleHandler::SetMakefile ( fMakefile );
}

void
MingwBackend::CloseMakefile () const
{
	if (fMakefile)
		fclose ( fMakefile );
}

void
MingwBackend::GenerateHeader () const
{
	fprintf ( fMakefile, "# THIS FILE IS AUTOMATICALLY GENERATED, EDIT 'ReactOS.xml' INSTEAD\n\n" );
}

void
MingwBackend::GenerateProjectCFlagsMacro ( const char* assignmentOperation,
                                           const vector<Include*>& includes,
                                           const vector<Define*>& defines ) const
{
	size_t i;

	fprintf (
		fMakefile,
		"PROJECT_CFLAGS %s",
		assignmentOperation );
	for ( i = 0; i < includes.size(); i++ )
	{
		fprintf (
			fMakefile,
			" -I%s",
			includes[i]->directory.c_str() );
	}

	for ( i = 0; i < defines.size(); i++ )
	{
		Define& d = *defines[i];
		fprintf (
			fMakefile,
			" -D%s",
			d.name.c_str() );
		if ( d.value.size() )
			fprintf (
				fMakefile,
				"=%s",
				d.value.c_str() );
	}
	fprintf ( fMakefile, "\n" );
}

void
MingwBackend::GenerateGlobalCFlagsAndProperties (
	const char* assignmentOperation,
	const vector<Property*>& properties,
	const vector<Include*>& includes,
	const vector<Define*>& defines,
	const vector<If*>& ifs ) const
{
	size_t i;

	for ( i = 0; i < properties.size(); i++ )
	{
		Property& prop = *properties[i];
		fprintf ( fMakefile, "%s := %s\n",
			prop.name.c_str(),
			prop.value.c_str() );
	}

	if ( includes.size() || defines.size() )
	{
		GenerateProjectCFlagsMacro ( assignmentOperation,
                                     includes,
                                     defines );
	}

	for ( i = 0; i < ifs.size(); i++ )
	{
		If& rIf = *ifs[i];
		if ( rIf.defines.size() || rIf.includes.size() || rIf.ifs.size() )
		{
			fprintf (
				fMakefile,
				"ifeq (\"$(%s)\",\"%s\")\n",
				rIf.property.c_str(),
				rIf.value.c_str() );
			GenerateGlobalCFlagsAndProperties (
				"+=",
				rIf.properties,
				rIf.includes,
				rIf.defines,
				rIf.ifs );
			fprintf (
				fMakefile,
				"endif\n\n" );
		}
	}
}

string
MingwBackend::GenerateProjectLFLAGS () const
{
	string lflags;
	for ( size_t i = 0; i < ProjectNode.linkerFlags.size (); i++ )
	{
		LinkerFlag& linkerFlag = *ProjectNode.linkerFlags[i];
		if ( lflags.length () > 0 )
			lflags += " ";
		lflags += linkerFlag.flag;
	}
	return lflags;
}

void
MingwBackend::GenerateGlobalVariables () const
{
	fprintf ( fMakefile, "mkdir = tools" SSEP "rmkdir" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "winebuild = tools" SSEP "winebuild" SSEP "winebuild" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "bin2res = tools" SSEP "bin2res" SSEP "bin2res" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "cabman = tools" SSEP "cabman" SSEP "cabman" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "cdmake = tools" SSEP "cdmake" SSEP "cdmake" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "rsym = tools" SSEP "rsym" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "wrc = tools" SSEP "wrc" SSEP "wrc" EXEPOSTFIX "\n" );
	fprintf ( fMakefile, "\n" );
	GenerateGlobalCFlagsAndProperties (
		"=",
		ProjectNode.properties,
		ProjectNode.includes,
		ProjectNode.defines,
		ProjectNode.ifs );
	fprintf ( fMakefile, "PROJECT_RCFLAGS = $(PROJECT_CFLAGS)\n" );
	fprintf ( fMakefile, "PROJECT_LFLAGS = %s\n",
	          GenerateProjectLFLAGS ().c_str () );
	fprintf ( fMakefile, "\n" );
}

bool
MingwBackend::IncludeInAllTarget ( const Module& module ) const
{
	if ( module.type == ObjectLibrary )
		return false;
	if ( module.type == BootSector )
		return false;
	if ( module.type == Iso )
		return false;
	return true;
}

void
MingwBackend::GenerateAllTarget () const
{
	fprintf ( fMakefile, "all:" );
	for ( size_t i = 0; i < ProjectNode.modules.size (); i++ )
	{
		Module& module = *ProjectNode.modules[i];
		if ( IncludeInAllTarget ( module ) )
		{
			fprintf ( fMakefile,
			          " %s",
			          FixupTargetFilename ( module.GetPath () ).c_str () );
		}
	}
	fprintf ( fMakefile, "\n\t\n\n" );
}

string
MingwBackend::GetBuildToolDependencies () const
{
	string dependencies;
	for ( size_t i = 0; i < ProjectNode.modules.size (); i++ )
	{
		Module& module = *ProjectNode.modules[i];
		if ( module.type == BuildTool )
		{
			if ( dependencies.length () > 0 )
				dependencies += " ";
			dependencies += module.GetDependencyPath ();
		}
	}
	return dependencies;
}

void
MingwBackend::GenerateInitTarget () const
{
	fprintf ( fMakefile,
	          "init:");
	fprintf ( fMakefile,
	          " $(ROS_INTERMEDIATE)." SSEP "tools" );
	fprintf ( fMakefile,
	          " %s",
	          GetBuildToolDependencies ().c_str () );
	fprintf ( fMakefile,
	          " %s",
	          "include" SSEP "reactos" SSEP "buildno.h" );
	fprintf ( fMakefile,
	          "\n\t\n\n" );

	fprintf ( fMakefile,
	          "$(ROS_INTERMEDIATE)." SSEP "tools:\n" );
	fprintf ( fMakefile,
	          "ifneq ($(ROS_INTERMEDIATE),)\n" );
	fprintf ( fMakefile,
	          "\t${nmkdir} $(ROS_INTERMEDIATE)\n" );
	fprintf ( fMakefile,
	          "endif\n" );
	fprintf ( fMakefile,
	          "\t${nmkdir} $(ROS_INTERMEDIATE)." SSEP "tools\n" );
	fprintf ( fMakefile,
	          "\n" );
}

void
MingwBackend::GenerateXmlBuildFilesMacro() const
{
	fprintf ( fMakefile,
	          "XMLBUILDFILES = %s \\\n",
	          ProjectNode.GetProjectFilename ().c_str () );
	string xmlbuildFilenames;
	int numberOfExistingFiles = 0;
	for ( size_t i = 0; i < ProjectNode.xmlbuildfiles.size (); i++ )
	{
		XMLInclude& xmlbuildfile = *ProjectNode.xmlbuildfiles[i];
		if ( !xmlbuildfile.fileExists )
			continue;
		numberOfExistingFiles++;
		if ( xmlbuildFilenames.length () > 0 )
			xmlbuildFilenames += " ";
		xmlbuildFilenames += NormalizeFilename ( xmlbuildfile.topIncludeFilename );
		if ( numberOfExistingFiles % 5 == 4 || i == ProjectNode.xmlbuildfiles.size () - 1 )
		{
			fprintf ( fMakefile,
			          "\t%s",
			          xmlbuildFilenames.c_str ());
			if ( i == ProjectNode.xmlbuildfiles.size () - 1 )
			{
				fprintf ( fMakefile,
				          "\n" );
			}
			else
			{
				fprintf ( fMakefile,
				          " \\\n",
				          xmlbuildFilenames.c_str () );
			}
			xmlbuildFilenames.resize ( 0 );
		}
		numberOfExistingFiles++;
	}
	fprintf ( fMakefile,
	          "\n" );
}

void
MingwBackend::CheckAutomaticDependencies ()
{
	AutomaticDependency automaticDependency ( ProjectNode );
	automaticDependency.Process ();
	automaticDependency.CheckAutomaticDependencies ();
}

void
MingwBackend::ProcessModule ( Module& module ) const
{
	MingwModuleHandler* h = MingwModuleHandler::LookupHandler (
		module.node.location,
		module.type );
	h->Process ( module );
	h->GenerateDirectoryTargets ();
}

string
FixupTargetFilename ( const string& targetFilename )
{
	return string("$(ROS_INTERMEDIATE)") + NormalizeFilename ( targetFilename );
}

void
MingwBackend::DetectPCHSupport()
{
	string path = "tools" SSEP "rbuild" SSEP "backend" SSEP "mingw" SSEP "pch_detection.h";
	system ( ssprintf("gcc -c %s", path.c_str()).c_str() );
	path += ".gch";
	{
		FILE* f = fopen ( path.c_str(), "rb" );
		if ( f )
		{
			use_pch = true;
			fclose(f);
			unlink ( path.c_str() );
		}
		else
			use_pch = false;
	}
	// TODO FIXME - eventually check for ROS_USE_PCH env var and
	// allow that to override use_pch if true
}
