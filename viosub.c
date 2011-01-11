#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_VIO
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dosqss.h"

#include "viosub.h"
#include "kshell.h"

#define BUF_SIZE    10240

static CHAR     m_achSysState[ BUF_SIZE ];

static ULONG    m_ulSGID = ( ULONG )-1;
static CHAR     m_szPipeName[ PIPE_KSHELL_VIOSUB_LEN ];

extern APIRET APIENTRY DosQuerySysState (ULONG func,
                ULONG par1, ULONG pid, ULONG _reserved_,
                PVOID buf,
                ULONG bufsz);

int  _CRT_init( void );
void _CRT_term( void );

ULONG APIENTRY getSGID( VOID )
{
    PQTOPLEVEL  pQTopLevel = ( PQTOPLEVEL )m_achSysState;
    PPIB        ppib;

    if( m_ulSGID != ( ULONG )-1 )
        return m_ulSGID;

    DosGetInfoBlocks( NULL, &ppib );

    DosQuerySysState( 0x01, 0, ppib->pib_ulpid, 1, pQTopLevel, BUF_SIZE );

    m_ulSGID = pQTopLevel->procdata->sessid;

    return m_ulSGID;
}

static VOID pipeOpen( HPIPE *phpipe )
{
    ULONG   ulAction;
    APIRET  rc;

    do
    {
        rc = DosOpen( m_szPipeName, phpipe, &ulAction, 0, 0,
                      OPEN_ACTION_OPEN_IF_EXISTS,
                      OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE |
                      OPEN_FLAGS_FAIL_ON_ERROR,
                      NULL );
        if( rc == ERROR_PIPE_BUSY )
            while( DosWaitNPipe( m_szPipeName, -1 ) == ERROR_INTERRUPT );
    } while( rc == ERROR_PIPE_BUSY );
}

#pragma pack( 2 )
typedef struct tagVIOSETCURPOSPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
} VIOSETCURPOSPARAM, *PVIOSETCURPOSPARAM;
#pragma pack()

static ULONG vioSetCurPos( USHORT usIndex, PVOID pargs )
{
    PVIOSETCURPOSPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOSETCURTYPEPARAM
{
    HVIO                    hvio;
    PVIOCURSORINFO  _Seg16  pvci;
} VIOSETCURTYPEPARAM, *PVIOSETCURTYPEPARAM;
#pragma pack()

static ULONG vioSetCurType( USHORT usIndex, PVOID pargs )
{
    PVIOSETCURTYPEPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pvci, sizeof( VIOCURSORINFO ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTNCHARPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usTimes;
    PCH     _Seg16  pch;
} VIOWRTNCHARPARAM, *PVIOWRTNCHARPARAM;
#pragma pack()

static ULONG vioWrtNChar( USHORT usIndex, PVOID pargs )
{
    PVIOWRTNCHARPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTimes, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pch, sizeof( CHAR ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTNATTRPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usTimes;
    PBYTE   _Seg16  pbAttr;
} VIOWRTNATTRPARAM, *PVIOWRTNATTRPARAM;
#pragma pack()

static ULONG vioWrtNAttr( USHORT usIndex, PVOID pargs )
{
    PVIOWRTNATTRPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTimes, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbAttr, sizeof( BYTE ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTNCELLPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usTimes;
    PBYTE   _Seg16  pbCell;
} VIOWRTNCELLPARAM, *PVIOWRTNCELLPARAM;
#pragma pack()

static ULONG vioWrtNCell( USHORT usIndex, PVOID pargs )
{
    PVIOWRTNCELLPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTimes, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTCHARSTRPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usLen;
    PCH     _Seg16  pchCharStr;
} VIOWRTCHARSTRPARAM, *PVIOWRTCHARSTRPARAM;
#pragma pack()

static ULONG vioWrtCharStr( USHORT usIndex, PVOID pargs )
{
    PVIOWRTCHARSTRPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLen, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pchCharStr, p->usLen, &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTCHARSTRATTPARAM
{
    HVIO            hvio;
    PBYTE   _Seg16  pbAttr;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usLen;
    PCH     _Seg16  pchCharStr;
} VIOWRTCHARSTRATTPARAM, *PVIOWRTCHARSTRATTPARAM;
#pragma pack()

static ULONG vioWrtCharStrAtt( USHORT usIndex, PVOID pargs )
{
    PVIOWRTCHARSTRATTPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbAttr, sizeof( BYTE ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLen, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pchCharStr, p->usLen, &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOWRTCELLSTRPARAM
{
    HVIO            hvio;
    USHORT          usCol;
    USHORT          usRow;
    USHORT          usLen;
    PCH     _Seg16  pchCellStr;
} VIOWRTCELLSTRPARAM, *PVIOWRTCELLSTRPARAM;
#pragma pack()

static ULONG vioWrtCellStr( USHORT usIndex, PVOID pargs )
{
    PVIOWRTCELLSTRPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLen, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pchCellStr, p->usLen * sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOSCROLLUPPARAM
{
    HVIO            hvio;
    PBYTE   _Seg16  pbCell;
    USHORT          usLines;
    USHORT          usRightCol;
    USHORT          usBotRow;
    USHORT          usLeftCol;
    USHORT          usTopRow;
} VIOSCROLLUPPARAM, *PVIOSCROLLUPPARAM;
#pragma pack()

static ULONG vioScrollUp( USHORT usIndex, PVOID pargs )
{
    PVIOSCROLLUPPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );
    DosWrite( hpipe, &p->usLines, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRightCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usBotRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLeftCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTopRow, sizeof( USHORT ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOSCROLLDNPARAM
{
    HVIO            hvio;
    PBYTE   _Seg16  pbCell;
    USHORT          usLines;
    USHORT          usRightCol;
    USHORT          usBotRow;
    USHORT          usLeftCol;
    USHORT          usTopRow;
} VIOSCROLLDNPARAM, *PVIOSCROLLDNPARAM;
#pragma pack()

static ULONG vioScrollDn( USHORT usIndex, PVOID pargs )
{
    PVIOSCROLLDNPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );
    DosWrite( hpipe, &p->usLines, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRightCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usBotRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLeftCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTopRow, sizeof( USHORT ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOSCROLLLFPARAM
{
    HVIO            hvio;
    PBYTE   _Seg16  pbCell;
    USHORT          usLines;
    USHORT          usRightCol;
    USHORT          usBotRow;
    USHORT          usLeftCol;
    USHORT          usTopRow;
} VIOSCROLLLFPARAM, *PVIOSCROLLLFPARAM;
#pragma pack()

static ULONG vioScrollLf( USHORT usIndex, PVOID pargs )
{
    PVIOSCROLLLFPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );
    DosWrite( hpipe, &p->usLines, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRightCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usBotRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLeftCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTopRow, sizeof( USHORT ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}

#pragma pack( 2 )
typedef struct tagVIOSCROLLRTPARAM
{
    HVIO            hvio;
    PBYTE   _Seg16  pbCell;
    USHORT          usLines;
    USHORT          usRightCol;
    USHORT          usBotRow;
    USHORT          usLeftCol;
    USHORT          usTopRow;
} VIOSCROLLRTPARAM, *PVIOSCROLLRTPARAM;
#pragma pack()

static ULONG vioScrollRt( USHORT usIndex, PVOID pargs )
{
    PVIOSCROLLRTPARAM p = pargs;

    HPIPE   hpipe;
    ULONG   cbActual;

    pipeOpen( &hpipe );

    DosWrite( hpipe, &usIndex, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, p->pbCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );
    DosWrite( hpipe, &p->usLines, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usRightCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usBotRow, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usLeftCol, sizeof( USHORT ), &cbActual );
    DosWrite( hpipe, &p->usTopRow, sizeof( USHORT ), &cbActual );

    DosClose( hpipe );

    return ( ULONG )-1;
}


#pragma pack( 2 )
typedef struct vioargs {
    USHORT es_reg;
    USHORT ds_reg;
    ULONG  think16_addr;
    USHORT CallerDS;
    USHORT NearAddr;
    USHORT Index;
    ULONG  CallerAddr;
    USHORT VioHandle;
} vioargs;
#pragma pack()

ULONG __cdecl Entry32Main( vioargs * args )
{
    switch( args->Index )
    {
        //case VI_VIOSHOWBUF       : return vioShowBuf( args->Index, &args->VioHandle );
        case VI_VIOSETCURPOS     : return vioSetCurPos( args->Index, &args->VioHandle);
        case VI_VIOSETCURTYPE    : return vioSetCurType( args->Index, &args->VioHandle );
        //case VI_VIOSETMODE       : return vioSetMode( args->Index, &args->VioHandle );
        case VI_VIOWRTNCHAR      : return vioWrtNChar( args->Index, &args->VioHandle );
        case VI_VIOWRTNATTR      : return vioWrtNAttr( args->Index, &args->VioHandle );
        case VI_VIOWRTNCELL      : return vioWrtNCell( args->Index, &args->VioHandle );
        case VI_VIOWRTCHARSTR    : return vioWrtCharStr( args->Index, &args->VioHandle );
        case VI_VIOWRTCHARSTRATT : return vioWrtCharStrAtt( args->Index, &args->VioHandle );
        case VI_VIOWRTCELLSTR    : return vioWrtCellStr( args->Index, &args->VioHandle );
        case VI_VIOSCROLLUP      : return vioScrollUp( args->Index, &args->VioHandle );
        case VI_VIOSCROLLDN      : return vioScrollDn( args->Index, &args->VioHandle );
        case VI_VIOSCROLLLF      : return vioScrollLf( args->Index, &args->VioHandle );
        case VI_VIOSCROLLRT      : return vioScrollRt( args->Index, &args->VioHandle );
        //case VI_VIOPOPUP         : return vioPopUp( args->Index, &args->VioHandle );
        //case VI_VIOENDPOPUP      : return vioEndPopUp( args->Index, &args->VioHandle );
    }

    return ( ULONG )-1;
}

unsigned long _System _DLL_InitTerm( unsigned long hmod, unsigned long ulFlag )
{
    switch( ulFlag )
    {
        case 0 : /* for initialize */
            if( _CRT_init() == -1 )
                return 0UL;

            getSGID();

            strcpy( m_szPipeName, PIPE_KSHELL_VIOSUB_BASE );
            _ultoa( m_ulSGID, m_szPipeName + strlen( m_szPipeName ), 16 );
            break;

        case 1 : /* for termination */
            _CRT_term();
            break;

        default :
            return 0UL;
    }

    return 1UL;
}

