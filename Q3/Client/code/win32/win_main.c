/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// win_main.c

#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"
#include "glw_win.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
//#include <io.h>
//#include <conio.h>

#define MEM_THRESHOLD 96*1024*1024

WinVars_t	g_wv;

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void ) {
	MEMORYSTATUS stat;
	GlobalMemoryStatus( &stat );
	return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
}


/*
==================
Sys_BeginProfiling
==================
*/
void Sys_BeginProfiling( void ) {
	// this is just used on the mac build
}


/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void QDECL Sys_Error( const char *error, ... ) {
	va_list		argptr;
	char		text[4096];
    MSG        msg;

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	Conbuf_AppendText( text );
	Conbuf_AppendText( "\n" );

	Sys_SetErrorText( text );
	Sys_ShowConsole( 1, qtrue );

	timeEndPeriod( 1 );

#ifndef DEDICATED
	IN_Shutdown();
#endif

	// wait for the user to quit
	while ( 1 ) {
		if ( GetMessage( &msg, NULL, 0, 0 ) <= 0 )
			Com_Quit_f();
		TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sys_DestroyConsole();

	exit (1);
}


/*
==============
Sys_Quit
==============
*/
void Sys_Quit( void ) {

	timeEndPeriod( 1 );

#ifndef DEDICATED
	IN_Shutdown();
#endif

	Sys_DestroyConsole();
	exit (0);
}


/*
==============
Sys_Print
==============
*/
void Sys_Print( const char *msg ) 
{
	Conbuf_AppendText( msg );
}


/*
==============
Sys_Mkdir
==============
*/
void Sys_Mkdir( const char *path ) 
{
	_mkdir( path );
}


/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen( const char *ospath, const char *mode )
{
	return fopen( ospath, mode );
}


/*
==============
Sys_Pwd
==============
*/
const char *Sys_Pwd( void )
{
	static char pwd[ MAX_OSPATH ];
	TCHAR	buffer[ MAX_OSPATH ];
	char *s;

	if ( pwd[0] )
		return pwd;

	GetModuleFileName( NULL, buffer, ARRAY_LEN( buffer ) -1 );
	buffer[ ARRAY_LEN( buffer ) - 1 ] = '\0';

	strcpy( pwd, WtoA( buffer ) );

	s = strrchr( pwd, PATH_SEP );
	if ( s ) 
		*s = '\0';
	else // bogus case?
	{
		_getcwd( pwd, sizeof( pwd ) - 1 );
		pwd[ sizeof( pwd ) - 1 ] = '\0';
	}

	return pwd;
}


/*
==============
Sys_DefaultBasePath
==============
*/
const char *Sys_DefaultBasePath( void ) 
{
	return Sys_Pwd();
}


/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define	MAX_FOUND_FILES	0x1000

void Sys_ListFilteredFiles( const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles ) {
	char		search[MAX_OSPATH*2+1];
	char		newsubdirs[MAX_OSPATH*2];
	char		filename[MAX_OSPATH*2];
	intptr_t	findhandle;
	struct _finddata_t findinfo;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if ( *subdirs ) {
		Com_sprintf( search, sizeof(search), "%s\\%s\\*", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof(search), "%s\\*", basedir );
	}

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		return;
	}

	do {
		if (findinfo.attrib & _A_SUBDIR) {
			if ( !Q_streq( findinfo.name, "." ) && !Q_streq( findinfo.name, ".." ) ) {
				if ( *subdirs ) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name );
				} else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", findinfo.name );
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name );
		if (!Com_FilterPath( filter, filename, qfalse ))
			continue;
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	} while ( _findnext (findhandle, &findinfo) != -1 );

	_findclose (findhandle);
}

static qboolean strgtr(const char *s0, const char *s1) {
	int l0, l1, i;

	l0 = strlen(s0);
	l1 = strlen(s1);

	if (l1<l0) {
		l0 = l1;
	}

	for(i=0;i<l0;i++) {
		if (s1[i] > s0[i]) {
			return qtrue;
		}
		if (s1[i] < s0[i]) {
			return qfalse;
		}
	}
	return qfalse;
}


/*
=============
Sys_Sleep
=============
*/
void Sys_Sleep( int msec ) {

	if ( msec < 0 && com_dedicated->integer ) {
		WaitMessage();
		return;
	}

	if ( msec <= 0 ) {
		return;
	}

	Sleep ( msec );
}


/*
=============
Sys_ListFiles
=============
*/
char **Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs ) {
	char		search[MAX_OSPATH*2+MAX_QPATH+1];
	int			nfiles;
	char		**listCopy;
	char		*list[MAX_FOUND_FILES];
	struct _finddata_t findinfo;
	intptr_t	findhandle;
	int			flag;
	int			i;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = 0;
		*numfiles = nfiles;

		if (!nfiles)
			return NULL;

		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension ) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	Com_sprintf( search, sizeof(search), "%s\\*%s", directory, extension );

	// search
	nfiles = 0;

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		*numfiles = 0;
		return NULL;
	}

	do {
		if ( (!wantsubs && flag ^ ( findinfo.attrib & _A_SUBDIR )) || (wantsubs && findinfo.attrib & _A_SUBDIR) ) {
			if ( nfiles == MAX_FOUND_FILES - 1 ) {
				break;
			}
			list[ nfiles ] = CopyString( findinfo.name );
			nfiles++;
		}
	} while ( _findnext (findhandle, &findinfo) != -1 );

	list[ nfiles ] = 0;

	_findclose (findhandle);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	do {
		flag = 0;
		for(i=1; i<nfiles; i++) {
			if (strgtr(listCopy[i-1], listCopy[i])) {
				char *temp = listCopy[i];
				listCopy[i] = listCopy[i-1];
				listCopy[i-1] = temp;
				flag = 1;
			}
		}
	} while(flag);

	return listCopy;
}


/*
=============
Sys_FreeFileList
=============
*/
void Sys_FreeFileList( char **list ) {
	int		i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

//========================================================


/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void ) {
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) ) {
		HANDLE hClipboardData;
		DWORD size;

		// GetClipboardData performs implicit CF_UNICODETEXT => CF_TEXT conversion
		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 ) {
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) {
				size = GlobalSize( hClipboardData ) + 1;
				data = Z_Malloc( size );
				Q_strncpyz( data, cliptext, size );
				GlobalUnlock( hClipboardData );
				
				strtok( data, "\n\r\b" );
			}
		}
		CloseClipboard();
	}
	return data;
}


/*
========================================================================

LOAD/UNLOAD DLL

========================================================================
*/

static int dll_err_count = 0;

/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary( const char *name ) 
{
	const char *ext;

	if ( !name || !*name )
		return NULL;

	if ( FS_AllowedExtension( name, qfalse, &ext ) )
	{
		Com_Error( ERR_FATAL, "Sys_LoadLibrary: Unable to load library with '%s' extension", ext );
	}

	return (void *)LoadLibrary( AtoW( name ) );
}


/*
=================
Sys_LoadFunction
=================
*/
void *Sys_LoadFunction( void *handle, const char *name ) 
{
	void *symbol;

	if ( handle == NULL || name == NULL || *name == '\0' ) 
	{
		dll_err_count++;
		return NULL;
	}

	symbol = GetProcAddress( handle, name );
	if ( !symbol )
		dll_err_count++;

	return symbol;
}


/*
=================
Sys_LoadFunctionErrors
=================
*/
int Sys_LoadFunctionErrors( void ) 
{
	int result = dll_err_count;
	dll_err_count = 0;
	return result;
}


/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary( void *handle ) 
{
	if ( handle ) 
		FreeLibrary( handle );
}


/*
=================
Sys_UnloadDll

=================
*/
void Sys_UnloadDll( void *dllHandle ) {
	if ( !dllHandle ) {
		return;
	}
	if ( !FreeLibrary( dllHandle ) ) {
		Com_Error( ERR_FATAL, "Sys_UnloadDll FreeLibrary failed" );
	}
}


/*
=================
Sys_LoadDll

Used to load a development dll instead of a virtual machine

TTimo: added some verbosity in debug
=================
*/
void * QDECL Sys_LoadDll( const char *name, dllSyscall_t *entryPoint, dllSyscall_t systemcalls ) {

	HINSTANCE	libHandle;
	dllEntry_t	dllEntry;
#ifdef DEBUG
	TCHAR		currpath[ MAX_OSPATH ];
#endif
	const char	*basepath;
	const char	*homepath;
	const char	*gamedir;
	char		*fn;
	char		filename[ MAX_QPATH ];

#if idx64
	Com_sprintf( filename, sizeof( filename ), "%sx86_64.dll", name );
#else
	Com_sprintf( filename, sizeof( filename ), "%sx86.dll", name );
#endif

	basepath = Cvar_VariableString( "fs_basepath" );
	homepath = Cvar_VariableString( "fs_homepath" );
	gamedir = Cvar_VariableString( "fs_game" );
	if ( !*gamedir ) {
		gamedir = Cvar_VariableString( "fs_basegame" );
	}
	fn = filename;

#ifdef DEBUG
	if ( GetCurrentDirectory( currpath, ARRAY_LEN( currpath ) ) < ARRAY_LEN( currpath ) ) {
		fn = FS_BuildOSPath( WtoA( currpath ), gamedir, filename );
		libHandle = LoadLibrary( AtoW( filename ) );
	} else
#endif
	libHandle = NULL;

	if ( !libHandle && *homepath ) {
		fn = FS_BuildOSPath( homepath, gamedir, filename );
		libHandle = LoadLibrary( AtoW( fn ) );
	}

	if ( !libHandle && Q_stricmp( basepath, homepath ) ) {
		fn = FS_BuildOSPath( basepath, gamedir, filename );
		libHandle = LoadLibrary( AtoW( fn ) );
	}

	if ( !libHandle ) {
		Com_Printf( "LoadLibrary '%s' failed\n", fn );
		return NULL;
	}

	Com_Printf( "LoadLibrary '%s' ok\n", fn );

	dllEntry = ( dllEntry_t ) GetProcAddress( libHandle, "dllEntry" ); 
	*entryPoint = ( dllSyscall_t ) GetProcAddress( libHandle, "vmMain" );
	if ( !*entryPoint || !dllEntry ) {
		FreeLibrary( libHandle );
		return NULL;
	}

	Com_Printf( "Sys_LoadDll(%s) found **vmMain** at %p\n", name, *entryPoint );
	dllEntry( systemcalls );
	Com_Printf( "Sys_LoadDll(%s) succeeded!\n", name );

	return libHandle;
}


/*
=================
Sys_SendKeyEvents

Platform-dependent event handling
=================
*/
void Sys_SendKeyEvents( void ) 
{
    MSG	msg;
	// pump the message loop
	while ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) {
		if ( GetMessage( &msg, NULL, 0, 0 ) <= 0 ) {
			Com_Quit_f();
		}

		// save the msg time, because wndprocs don't have access to the timestamp
		//g_wv.sysMsgTime = msg.time;
		g_wv.sysMsgTime = Sys_Milliseconds();

		TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
}


//================================================================

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
#ifndef DEDICATED

void Sys_In_Restart_f( void ) {
	IN_Shutdown();
	IN_Init();
}

#endif


/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/
#define OSR2_BUILD_NUMBER 1111
#define WIN98_BUILD_NUMBER 1998

void Sys_Init( void ) {

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod( 1 );

#ifndef DEDICATED
	Cmd_AddCommand ("in_restart", Sys_In_Restart_f);
#endif

	g_wv.osversion.dwOSVersionInfoSize = sizeof( g_wv.osversion );

	if (!GetVersionEx (&g_wv.osversion))
		Sys_Error ("Couldn't get OS info");

	if (g_wv.osversion.dwMajorVersion < 4)
		Sys_Error ("Quake3 requires Windows version 4 or greater");
	if (g_wv.osversion.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("Quake3 doesn't run on Win32s");

	if ( g_wv.osversion.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		Cvar_Set( "arch", "winnt" );
	}
	else if ( g_wv.osversion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
	{
		if ( LOWORD( g_wv.osversion.dwBuildNumber ) >= WIN98_BUILD_NUMBER )
		{
			Cvar_Set( "arch", "win98" );
		}
		else if ( LOWORD( g_wv.osversion.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
		{
			Cvar_Set( "arch", "win95 osr2.x" );
		}
		else
		{
			Cvar_Set( "arch", "win95" );
		}
	}
	else
	{
		Cvar_Set( "arch", "unknown Windows variant" );
	}

	Cvar_Set( "username", Sys_GetCurrentUser() );

#ifndef DEDICATED
	glw_state.wndproc = MainWndProc;

	IN_Init();
#endif

}

//=======================================================================


/*
==================
SetDPIAwareness
==================
*/
static void SetDPIAwareness( void ) 
{
	typedef HANDLE (WINAPI *pfnSetThreadDpiAwarenessContext)( HANDLE dpiContext );
	pfnSetThreadDpiAwarenessContext pSetThreadDpiAwarenessContext;
	HMODULE dll;

	dll = GetModuleHandle( T("user32") );
	if ( !dll )  
		return;

	pSetThreadDpiAwarenessContext = (pfnSetThreadDpiAwarenessContext) GetProcAddress( dll, "SetThreadDpiAwarenessContext" );
	if ( !pSetThreadDpiAwarenessContext ) 
		return;

	pSetThreadDpiAwarenessContext( (HANDLE)(-2) ); // DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
}


/*
==================
WinMain
==================
*/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) 
{
	static char	sys_cmdline[ MAX_STRING_CHARS ];
	char con_title[ MAX_CVAR_VALUE_STRING ];
	int vid_xpos, vid_ypos;
	qboolean useXYpos;

	// should never get a previous instance in Win32
	if ( hPrevInstance ) {
		return 0;
	}

	SetDPIAwareness();

	g_wv.hInstance = hInstance;
	Q_strncpyz( sys_cmdline, lpCmdLine, sizeof( sys_cmdline ) );

	useXYpos = Com_EarlyParseCmdLine( sys_cmdline, con_title, sizeof( con_title ), &vid_xpos, &vid_ypos );

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole( con_title, vid_xpos, vid_ypos, useXYpos );

	// no abort/retry/fail errors
	SetErrorMode( SEM_FAILCRITICALERRORS );

	// get the initial time base
	Sys_Milliseconds();

	Com_Init( sys_cmdline );
	NET_Init();

	Com_Printf( "Working directory: %s\n", Sys_Pwd() );

	// hide the early console since we've reached the point where we
	// have a working graphics subsystems
	if ( !com_dedicated->integer && !com_viewlog->integer ) {
		Sys_ShowConsole( 0, qfalse );
	}

	// main game loop
	while( 1 ) {
		// set low precision every frame, because some system calls
		// reset it arbitrarily
		// _controlfp( _PC_24, _MCW_PC );
		// _controlfp( -1, _MCW_EM  ); // no exceptions, even if some crappy syscall turns them back on!

#ifdef DEDICATED
		// run the game
		Com_Frame( qfalse );
#else
		// make sure mouse and joystick are only called once a frame
		IN_Frame();
		// run the game
		Com_Frame( clc.demoplaying );
#endif
	}
	// never gets here
	return 0;
}
