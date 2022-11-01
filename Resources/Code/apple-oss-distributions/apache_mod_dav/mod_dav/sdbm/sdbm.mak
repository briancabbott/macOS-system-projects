# Microsoft Developer Studio Generated NMAKE File, Based on sdbm.dsp

!IF "$(APACHE)" == ""
APACHE=\apache-1.3
!MESSAGE No Apache directory specified, defaulting to $(APACHE)
!MESSAGE
!ENDIF

!IF "$(CFG)" == ""
CFG=sdbm - Win32 Release
!MESSAGE No configuration specified. Defaulting to sdbm - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "sdbm - Win32 Release" && "$(CFG)" != "sdbm - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "sdbm.mak" CFG="sdbm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "sdbm - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "sdbm - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "sdbm - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\sdbm.lib"


CLEAN :
	-@erase "$(INTDIR)\sdbm.obj"
	-@erase "$(INTDIR)\sdbm_hash.obj"
	-@erase "$(INTDIR)\sdbm_lock.obj"
	-@erase "$(INTDIR)\sdbm_pair.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\sdbm.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "$(APACHE)\src\include" /I "$(APACHE)\src\os\win32" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\sdbm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\sdbm.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\sdbm.lib" 
LIB32_OBJS= \
	"$(INTDIR)\sdbm.obj" \
	"$(INTDIR)\sdbm_hash.obj" \
	"$(INTDIR)\sdbm_lock.obj" \
	"$(INTDIR)\sdbm_pair.obj"

"$(OUTDIR)\sdbm.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "sdbm - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\sdbm.lib"


CLEAN :
	-@erase "$(INTDIR)\sdbm.obj"
	-@erase "$(INTDIR)\sdbm_hash.obj"
	-@erase "$(INTDIR)\sdbm_lock.obj"
	-@erase "$(INTDIR)\sdbm_pair.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\sdbm.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /I "$(APACHE)\src\os\win32" /I "$(APACHE)\src\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SHARED_MODULE" /Fp"$(INTDIR)\sdbm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\sdbm.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\sdbm.lib" /nodefaultlib 
LIB32_OBJS= \
	"$(INTDIR)\sdbm.obj" \
	"$(INTDIR)\sdbm_hash.obj" \
	"$(INTDIR)\sdbm_lock.obj" \
	"$(INTDIR)\sdbm_pair.obj"

"$(OUTDIR)\sdbm.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "sdbm - Win32 Release" || "$(CFG)" == "sdbm - Win32 Debug"
SOURCE=.\sdbm.c

"$(INTDIR)\sdbm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\sdbm_hash.c

"$(INTDIR)\sdbm_hash.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\sdbm_lock.c

"$(INTDIR)\sdbm_lock.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\sdbm_pair.c

"$(INTDIR)\sdbm_pair.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

