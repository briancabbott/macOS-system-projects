# Microsoft Developer Studio Generated NMAKE File, Based on dijkstra.dsp
!IF "$(CFG)" == ""
CFG=dijkstra - Win32 Debug
!MESSAGE No configuration specified. Defaulting to dijkstra - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "dijkstra - Win32 Release" && "$(CFG)" != "dijkstra - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dijkstra.mak" CFG="dijkstra - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dijkstra - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "dijkstra - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "dijkstra - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dijkstra.exe"

!ELSE 

ALL : "ToolsSupport - Win32 Release" "$(OUTDIR)\dijkstra.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"ToolsSupport - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dijkstra.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\dijkstra.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../src" /I "../.." /I "../../agraph" /I "../../cdt" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\dijkstra.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dijkstra.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=agraph.lib cdt.lib common.lib ToolsSupport.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\dijkstra.pdb" /machine:I386 /out:"$(OUTDIR)\dijkstra.exe" /libpath:"../../makearch/win32/static/Release" 
LINK32_OBJS= \
	"$(INTDIR)\dijkstra.obj" \
	"$(OUTDIR)\ToolsSupport.lib"

"$(OUTDIR)\dijkstra.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "ToolsSupport - Win32 Release" "$(OUTDIR)\dijkstra.exe"
   copy   .\Release\dijkstra.exe    ..\..\makearch\win32\static\Release\dijkstra.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "dijkstra - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dijkstra.exe"

!ELSE 

ALL : "ToolsSupport - Win32 Debug" "$(OUTDIR)\dijkstra.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"ToolsSupport - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dijkstra.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\dijkstra.exe"
	-@erase "$(OUTDIR)\dijkstra.ilk"
	-@erase "$(OUTDIR)\dijkstra.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "../src" /I "../.." /I "../../agraph" /I "../../cdt" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\dijkstra.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dijkstra.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=agraph.lib cdt.lib common.lib ToolsSupport.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\dijkstra.pdb" /debug /machine:I386 /out:"$(OUTDIR)\dijkstra.exe" /pdbtype:sept /libpath:"../../makearch/win32/static/Debug" 
LINK32_OBJS= \
	"$(INTDIR)\dijkstra.obj" \
	"$(OUTDIR)\ToolsSupport.lib"

"$(OUTDIR)\dijkstra.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "ToolsSupport - Win32 Debug" "$(OUTDIR)\dijkstra.exe"
   copy      .\Debug\dijkstra.exe     ..\..\makearch\win32\static\Debug\dijkstra.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("dijkstra.dep")
!INCLUDE "dijkstra.dep"
!ELSE 
!MESSAGE Warning: cannot find "dijkstra.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dijkstra - Win32 Release" || "$(CFG)" == "dijkstra - Win32 Debug"
SOURCE=..\src\dijkstra.c

"$(INTDIR)\dijkstra.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "dijkstra - Win32 Release"

"ToolsSupport - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Release" 
   cd "."

"ToolsSupport - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "dijkstra - Win32 Debug"

"ToolsSupport - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Debug" 
   cd "."

"ToolsSupport - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ENDIF 


!ENDIF 

