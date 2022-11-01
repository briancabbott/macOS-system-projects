# Microsoft Developer Studio Generated NMAKE File, Based on tred.dsp
!IF "$(CFG)" == ""
CFG=tred - Win32 Debug
!MESSAGE No configuration specified. Defaulting to tred - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "tred - Win32 Release" && "$(CFG)" != "tred - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tred.mak" CFG="tred - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tred - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "tred - Win32 Debug" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "tred - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\tred.exe"

!ELSE 

ALL : "ToolsSupport - Win32 Release" "$(OUTDIR)\tred.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"ToolsSupport - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\tred.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\tred.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../src" /I "../.." /I "../../graph" /I "../../cdt" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\tred.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\tred.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=graph.lib cdt.lib ToolsSupport.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\tred.pdb" /machine:I386 /out:"$(OUTDIR)\tred.exe" /libpath:"../../makearch/win32/static/Release" 
LINK32_OBJS= \
	"$(INTDIR)\tred.obj" \
	"$(OUTDIR)\ToolsSupport.lib"

"$(OUTDIR)\tred.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "ToolsSupport - Win32 Release" "$(OUTDIR)\tred.exe"
   copy  .\Release\tred.exe   ..\..\makearch\win32\static\Release\tred.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "tred - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\tred.exe"

!ELSE 

ALL : "ToolsSupport - Win32 Debug" "$(OUTDIR)\tred.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"ToolsSupport - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\tred.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\tred.exe"
	-@erase "$(OUTDIR)\tred.ilk"
	-@erase "$(OUTDIR)\tred.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "../src" /I "../.." /I "../../graph" /I "../../cdt" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\tred.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\tred.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=graph.lib cdt.lib ToolsSupport.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\tred.pdb" /debug /machine:I386 /out:"$(OUTDIR)\tred.exe" /pdbtype:sept /libpath:"../../makearch/win32/static/Debug" 
LINK32_OBJS= \
	"$(INTDIR)\tred.obj" \
	"$(OUTDIR)\ToolsSupport.lib"

"$(OUTDIR)\tred.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "ToolsSupport - Win32 Debug" "$(OUTDIR)\tred.exe"
   copy      .\Debug\tred.exe     ..\..\makearch\win32\static\Debug\tred.exe
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
!IF EXISTS("tred.dep")
!INCLUDE "tred.dep"
!ELSE 
!MESSAGE Warning: cannot find "tred.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "tred - Win32 Release" || "$(CFG)" == "tred - Win32 Debug"
SOURCE=..\src\tred.c

"$(INTDIR)\tred.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "tred - Win32 Release"

"ToolsSupport - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Release" 
   cd "."

"ToolsSupport - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\ToolsSupport.mak CFG="ToolsSupport - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "tred - Win32 Debug"

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

