#define INCL_VIO
#define INCL_DOSNLS
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#define INCL_DOSMEMMGR
#define INCL_DOSSESMGR
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kshell.h"
#include "viodmn.h"

#define TID_KSHELL  ( TID_USERMAX - 1 )

#define PRF_APP         "KSHELL"
#define PRF_KEY_CP      "CODEPAGE"
#define PRF_KEY_FONT    "FONT"
#define PRF_KEY_SIZE    "SIZE"

#define DEFAULT_CODEPAGE    0
#define DEFAULT_FONT_FACE   "GulimChe"
#define DEFAULT_CHAR_PELS   16L

static VIOMODEINFO  m_vmi;

static FATTRS   m_fat;
static FIXED    m_fxPointSize;
static ULONG    m_lCharWidth;
static ULONG    m_lCharHeight;
static ULONG    m_lMaxDescender;

static PBYTE    m_pVioBuf = NULL;
static CHAR     m_szPipeName[ PIPE_NAME_LEN ];
static CHAR     m_szPid[ 20 ];

static PID      m_pidVioDmn;
static ULONG    m_sidVioDmn;

static BOOL m_afDBCSLeadByte[ 256 ] = { FALSE, };

static BOOL init( VOID );
static VOID done( VOID );

static VOID initDBCSEnv( USHORT usCP );
static VOID initFrame( HWND hwndFrame );

#define     isDBCSLeadByte( uch ) ( m_afDBCSLeadByte[( uch )])

static BOOL callVioDmn( USHORT usMsg );

static BOOL startVioDmn( VOID );
static VOID waitVioDmn( VOID );

MRESULT EXPENTRY windowProc( HWND, ULONG, MPARAM, MPARAM );

INT main( VOID )
{
    HAB     hab;
    HMQ     hmq;
    ULONG   flFrameFlags;
    HWND    hwndFrame;
    HWND    hwndClient;
    QMSG    qm;
    int     result = 0;

    init();

    if( !startVioDmn())
    {
        result = 1;
        goto main_exit;
    }

    waitVioDmn();

    if( callVioDmn( MSG_VIOINFO ))
    {
        result = 2;
        goto main_exit;
    }

    memcpy( &m_vmi, m_pVioBuf, sizeof( VIOMODEINFO ));

    hab = WinInitialize( 0 );

    hmq = WinCreateMsgQueue( hab, 0);

    WinRegisterClass(
        hab,
        WC_KSHELL,
        windowProc,
        CS_SIZEREDRAW,
        sizeof( PVOID )
    );

    flFrameFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_TASKLIST | FCF_DLGBORDER;

    hwndFrame = WinCreateStdWindow(
                HWND_DESKTOP,               // parent window handle
                WS_VISIBLE,                 // frame window style
                &flFrameFlags,              // window style
                WC_KSHELL,                  // class name
                "KShell",                   // window title
                0L,                         // default client style
                NULLHANDLE,                 // resource in exe file
                ID_KSHELL,                  // frame window id
                &hwndClient                 // client window handle
                );

    if( hwndFrame != NULLHANDLE )
    {
        initFrame( hwndFrame );

        while( WinGetMsg( hab, &qm, NULLHANDLE, 0, 0 ))
            WinDispatchMsg( hab, &qm );

        WinDestroyWindow( hwndFrame );
    }

    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

main_exit:

    done();

    return result;
}

static VOID convertVio2Win( PRECTL prcl )
{
    prcl->xLeft = prcl->xLeft * m_lCharWidth;
    prcl->yBottom = ( m_vmi.row - prcl->yBottom - 1 ) * m_lCharHeight;
    prcl->xRight = ( prcl->xRight + 1 ) * m_lCharWidth;
    prcl->yTop = ( m_vmi.row - prcl->yTop ) * m_lCharHeight;
}

#define X_Win2Vio( x ) (( int )(( x ) / m_lCharWidth ))
#define Y_Win2Vio( y ) (( int )( m_vmi.row - (( y ) / m_lCharHeight ) - 1 ))

typedef struct tagKSHELLDATA
{
    USHORT  x;
    USHORT  y;
    VIOCURSORINFO ci;
    PVOID   pVioBuf;
} KSHELLDATA, *PKSHELLDATA;

static VOID setCursor( HWND hwnd, BOOL fCreate )
{
    PKSHELLDATA pKShellData = WinQueryWindowPtr( hwnd, 0 );

    WinShowCursor( hwnd, FALSE );
    WinDestroyCursor( hwnd );

    if( fCreate && ( pKShellData->ci.attr != ( USHORT )-1 ))
    {
        USHORT usCellHeight = m_vmi.vres / m_vmi.row;
        USHORT usStart = m_lCharHeight * ( pKShellData->ci.yStart + 1 ) / usCellHeight;
        USHORT usEnd = m_lCharHeight * ( pKShellData->ci.cEnd + 1 ) / usCellHeight;

        WinCreateCursor( hwnd, pKShellData->x * m_lCharWidth,
                               ( m_vmi.row - pKShellData->y - 1 ) * m_lCharHeight +
                               ( m_lCharHeight - usEnd ),
                               m_lCharWidth,
                               usEnd - usStart + 1,
                               CURSOR_FLASH,
                               NULL );
        WinShowCursor( hwnd, TRUE );
    }
}

MRESULT EXPENTRY windowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    PKSHELLDATA pKShellData = WinQueryWindowPtr( hwnd, 0 );

    switch( msg )
    {
        case WM_CREATE :
        {
            pKShellData = ( PKSHELLDATA )malloc( sizeof( KSHELLDATA ));
            memset( pKShellData, 0, sizeof( KSHELLDATA ));

            pKShellData->pVioBuf = malloc( KSHELL_BUFSIZE );
            memset( pKShellData->pVioBuf, 0, sizeof( KSHELL_BUFSIZE ));

            if( callVioDmn( MSG_DUMP ))
            {
                WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                return 0;
            }

            memcpy( pKShellData->pVioBuf, m_pVioBuf, VIO_CISIZE );
            memcpy( pKShellData->pVioBuf, m_pVioBuf + VIO_CISIZE, VIO_SCRSIZE );

            WinStartTimer( WinQueryAnchorBlock( hwnd ), hwnd, TID_KSHELL, 100 );

            WinSetWindowPtr( hwnd, 0, pKShellData );

            return 0;
        }

        case WM_DESTROY :
        {
            WinStopTimer( WinQueryAnchorBlock( hwnd ), hwnd, TID_KSHELL );

            free( pKShellData->pVioBuf );
            free( pKShellData );

            WinSetWindowPtr( hwnd, 0, NULL );

            return 0;
        }

        case WM_CLOSE :
            callVioDmn( MSG_QUIT );
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );

            return 0;

        case WM_SETFOCUS :
            setCursor( hwnd, ( BOOL )mp2 );
            break;

        case WM_TIMER :
        {
            PUSHORT pVioBufMem;
            PUSHORT pVioBufShell;
            int     x, y;
            int     xStart, xEnd;
            RECTL   rcl;

            if( SHORT1FROMMP( mp1 ) != TID_KSHELL )
                break;

            if( callVioDmn( MSG_DUMP ))
                WinPostMsg( hwnd, WM_QUIT, 0, 0 );

            if( memcmp( pKShellData, m_pVioBuf, VIO_CISIZE ) != 0 )
            {
                memcpy( pKShellData, m_pVioBuf, VIO_CISIZE );

                if( WinQueryFocus( HWND_DESKTOP ) == hwnd )
                    setCursor( hwnd, TRUE );
            }

            pVioBufMem = ( PUSHORT )( m_pVioBuf + VIO_CISIZE );
            pVioBufShell = ( PUSHORT )pKShellData->pVioBuf;

            for( y = 0; y < m_vmi.row; y++ )
            {
                xStart = xEnd = -1;

                for( x = 0; x < m_vmi.col; x++ )
                {
                    if( *pVioBufShell != *pVioBufMem )
                    {
                        *pVioBufShell = *pVioBufMem;

                        if( xStart == -1 )
                            xStart = x;

                        xEnd = x;
                    }
                    else if( xStart != -1 )
                    {
                        rcl.xLeft = xStart;
                        rcl.yBottom = rcl.yTop = y;
                        rcl.xRight = xEnd;
                        convertVio2Win( &rcl );
                        WinInvalidateRect( hwnd, &rcl, FALSE );

                        xStart = xEnd = -1;
                    }

                    pVioBufShell++;
                    pVioBufMem++;
                }

                if( xStart != -1 )
                {
                    rcl.xLeft = xStart;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = xEnd;
                    convertVio2Win( &rcl );
                    WinInvalidateRect( hwnd, &rcl, FALSE );
                }
            }

            return 0;
        }

#if 0
        case WM_ERASEBACKGROUND :
        {
            RECTL   rcl;

            memcpy( &rcl, ( void * )mp2, sizeof( RECTL ));
            WinMapWindowPoints( WinQueryWindow( hwnd, QW_PARENT ), hwnd, ( PPOINTL )&rcl, 2 );
            WinInvalidateRect( hwnd, &rcl, FALSE );
            return 0;
        }
#endif
        case WM_PAINT :
        {
            RECTL   rcl;
            HPS     hps;
            SIZEF   sizef;
            int     xStart, yStart;
            int     xEnd, yEnd;

            hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl );

            if( WinIsRectEmpty( WinQueryAnchorBlock( hwnd ), &rcl ))
            {
                WinEndPaint( hps );

                return 0;
            }

            xStart = X_Win2Vio( rcl.xLeft );
            yStart = Y_Win2Vio( rcl.yTop - 1 );
            xEnd = X_Win2Vio( rcl.xRight - 1 );
            yEnd = Y_Win2Vio( rcl.yBottom );

#if 0
            if( xStart < 0 )
                xStart = 0;

            if( yStart < 0 )
                yStart = 0;

            if( xEnd >= m_vmi.col )
                xEnd = m_vmi.col - 1;

            if( yEnd >= m_vmi.row )
                yEnd = m_vmi.row - 1;
#endif
            GpiCreateLogFont( hps, NULL, 1L, &m_fat );

            GpiSetCharSet( hps, 1L );

            sizef.cx = m_fxPointSize;
            sizef.cy = m_fxPointSize;

            GpiSetCharBox( hps, &sizef );
            GpiSetBackMix( hps, BM_OVERPAINT );

            {
                int     x, y;
                POINTL  ptl;
                PUSHORT pVioBufShell;
                UCHAR   uchAttr;
                USHORT  usChar;
                USHORT  usLen;

                ptl.y = ( m_vmi.row - yStart - 1 ) * m_lCharHeight + m_lMaxDescender;
                for( y = yStart; y <= yEnd; y++ )
                {
                    ptl.x = xStart * m_lCharWidth;
                    pVioBufShell = (( PUSHORT )pKShellData->pVioBuf )
                                    + ( y * m_vmi.col ) + xStart;

                    for( x = xStart; x <= xEnd; x++ )
                    {
                        uchAttr = HIUCHAR( *pVioBufShell );
                        GpiSetColor( hps, ( uchAttr & 0xF0 ) >> 4 );
                        GpiSetBackColor( hps, uchAttr & 0x0F );

                        usChar = LOUCHAR( *pVioBufShell );
                        if( isDBCSLeadByte(( UCHAR )usChar ))
                        {
                            pVioBufShell++;
                            usChar = MAKEUSHORT( usChar, LOUCHAR( *pVioBufShell ));
                            usLen = 2;
                        }
                        else
                            usLen = 1;

                        GpiCharStringAt( hps, &ptl, usLen, ( PCH )&usChar );

                        ptl.x += m_lCharWidth * usLen;

                        pVioBufShell++;
                    }

                    ptl.y -= m_lCharHeight;
                }
            }

            WinEndPaint( hps );

            return 0;
        }

        case WM_TRANSLATEACCEL :
        {
            PQMSG pQmsg = ( PQMSG )mp1;

            if( pQmsg->msg == WM_CHAR )
                return 0;

            break;
        }

        case WM_CHAR :
        {
            PMPARAM pmp;
            BYTE    abKbdState[ 256 ];
            BYTE    abPhysKbdState[ 256 ];

            WinSetKeyboardStateTable( HWND_DESKTOP, abKbdState, FALSE );

            memset( abPhysKbdState, 0, sizeof( abPhysKbdState ));

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_LSHIFT ) & 0x8000 )
                abPhysKbdState[ PM_SC_LSHIFT ] = 0x80;

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_RSHIFT ) & 0x8000 )
                abPhysKbdState[ PM_SC_RSHIFT ] = 0x80;

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_LCTRL ) & 0x8000 )
                abPhysKbdState[ PM_SC_LCTRL ] = 0x80;

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_LALT ) & 0x8000 )
                abPhysKbdState[ PM_SC_LALT ] = 0x80;

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_RCTRL ) & 0x8000 )
                abPhysKbdState[ PM_SC_RCTRL ] = 0x80;

            if( WinGetPhysKeyState( HWND_DESKTOP, PM_SC_RALT ) & 0x8000 )
                abPhysKbdState[ PM_SC_RALT ] = 0x80;

            pmp = ( PMPARAM )m_pVioBuf;
            *pmp++ = mp1;
            *pmp++ = mp2;
            memcpy( pmp, abKbdState, sizeof( abKbdState ));
            memcpy((( PBYTE )pmp ) + sizeof( abKbdState ), abPhysKbdState, sizeof( abPhysKbdState ));

            if( callVioDmn( MSG_CHAR ))
                WinPostMsg( hwnd, WM_QUIT, 0, 0 );

            return MRFROMLONG( TRUE );
        }

        case WM_QUERYCONVERTPOS :
        {
            PRECTL  pCursorPos = ( PRECTL )mp1;

            pCursorPos->xRight = pCursorPos->xLeft = pKShellData->x;
            pCursorPos->yTop = pCursorPos->yBottom = pKShellData->y;

            convertVio2Win( pCursorPos );

            return MRFROMLONG( QCP_CONVERT );
        }

        case WM_COMMAND :
            switch( SHORT1FROMMP( mp1 ))
            {
                case IDM_CODEPAGE :
                {
                    HWND    hwndDlg;

                    hwndDlg = WinLoadDlg( HWND_DESKTOP, hwnd, WinDefDlgProc, NULLHANDLE, IDD_CODEPAGE, NULL );
                    if( hwndDlg )
                    {
                        HWND    hwndEF = WinWindowFromID( hwndDlg, IDEF_CODEPAGE );
                        CHAR    szCP[ 10 ];
                        ULONG   ulReply;

                        _itoa( m_fat.usCodePage, szCP, 10 );
                        WinSetWindowText( hwndEF, szCP );
                        WinSendMsg( hwndEF, EM_SETSEL, MPFROM2SHORT( 0, -1 ), 0 );

                        ulReply = WinProcessDlg( hwndDlg );

                        if( ulReply == DID_OK )
                        {

                            WinQueryWindowText( hwndEF, sizeof( szCP ), szCP );
                            m_fat.usCodePage = atoi( szCP );
                            initDBCSEnv( m_fat.usCodePage );

                            PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_CP, szCP );

                            WinInvalidateRect( hwnd, NULL, FALSE );
                        }

                        WinDestroyWindow( hwndDlg );
                    }

                    return 0;
                }

                case IDM_FONT :
                {
                    static FONTDLG fd;

                    HPS    hps;
                    HWND   hwndFontDlg;

                    memset( &fd, 0, sizeof( FONTDLG ));

                    hps = WinGetPS( hwnd );

                    fd.cbSize=sizeof(FONTDLG);
                    fd.hpsScreen = hps;

                    fd.pszFamilyname = m_fat.szFacename;
                    fd.usFamilyBufLen = sizeof( m_fat.szFacename );

                    fd.fxPointSize = m_fxPointSize;
                    fd.fl = FNTS_HELPBUTTON | FNTS_CENTER |
                            FNTS_FIXEDWIDTHONLY | FNTS_INITFROMFATTRS;
                    fd.clrFore = SYSCLR_WINDOWTEXT;
                    fd.clrBack = SYSCLR_WINDOW;

                    memcpy( &fd.fAttrs, &m_fat, sizeof( FATTRS ));

                    hwndFontDlg = WinFontDlg( HWND_DESKTOP, hwnd, &fd );

                    if( hwndFontDlg &&( fd.lReturn == DID_OK ))
                    {
                        CHAR    szPointSize[ 10 ];

                        memcpy( &m_fat, &fd.fAttrs, sizeof( FATTRS ));
                        m_fxPointSize = fd.fxPointSize;
                        _itoa( FIXEDINT( m_fxPointSize ), szPointSize, 10 );

                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_FONT, m_fat.szFacename );
                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_SIZE, szPointSize );

                        initFrame( WinQueryWindow( hwnd, QW_PARENT ));
                        setCursor( hwnd, TRUE );
                    }

                    WinReleasePS( hps );

                    return 0;
                }
            }
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

BOOL init( VOID )
{
    PPIB    ppib;
    CHAR    szName[ KSHELL_MEMNAME_LEN ];
    LONG    lResult;

    DosGetInfoBlocks( NULL, &ppib );

    _ultoa( ppib->pib_ulpid, m_szPid, 16 );

    strcpy( szName, KSHELL_VIOBUF_BASE );
    strcat( szName, m_szPid );

    DosAllocSharedMem(( PPVOID )&m_pVioBuf, szName, KSHELL_BUFSIZE,
                      PAG_READ | PAG_WRITE | PAG_COMMIT );

    strcpy( m_szPipeName, PIPE_VIO_DUMP_BASE );
    strcat( m_szPipeName, m_szPid );

    srandom( time( NULL ));

    m_fat.usRecordLength = sizeof( FATTRS );
    m_fat.fsSelection = 0;
    m_fat.lMatch = 0L;
    m_fat.idRegistry = 0;
    m_fat.usCodePage = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_CP, DEFAULT_CODEPAGE );
    m_fat.lMaxBaselineExt = 0;
    m_fat.lAveCharWidth = 0;
    m_fat.fsType = FATTR_TYPE_MBCS | FATTR_TYPE_DBCS;
    m_fat.fsFontUse = FATTR_FONTUSE_NOMIX;

    PrfQueryProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_FONT, DEFAULT_FONT_FACE, m_fat.szFacename, FACESIZE );

    lResult = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_SIZE, DEFAULT_CHAR_PELS );
    m_fxPointSize = MAKEFIXED( lResult ? lResult : DEFAULT_CHAR_PELS, 0 );

    initDBCSEnv( m_fat.usCodePage );

    return TRUE;
}

VOID done( VOID )
{
    DosFreeMem( m_pVioBuf );
}

VOID initDBCSEnv( USHORT usCP )
{
    COUNTRYCODE cc;
    UCHAR       uchDBCSInfo[ 12 ];
    int         i, j;

    cc.country = 0;
    cc.codepage = usCP ;

    if( DosQueryDBCSEnv( sizeof( uchDBCSInfo ), &cc, uchDBCSInfo ) == 0 )
    {
        for( i = 0; uchDBCSInfo[ i ] != 0 || uchDBCSInfo[ i + 1 ] != 0; i += 2 )
        {
            for( j = uchDBCSInfo[ i ]; j <= uchDBCSInfo[ i + 1 ]; j++ )
                m_afDBCSLeadByte[ j ] = TRUE;
        }
    }
}

static VOID randomizeRect( PRECTL prcl )
{
    LONG    cxScreen = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    LONG    cyScreen = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    LONG    newX, newY;

    newX = ( random() % ( cxScreen - ( prcl->xRight - prcl->xLeft )));
    newY = ( random() % ( cyScreen - ( prcl->yTop - prcl->yBottom )));

    prcl->xRight = newX + ( prcl->xRight - prcl->xLeft );
    prcl->yTop = newY + ( prcl->yTop - prcl->yBottom );

    prcl->xLeft = newX;
    prcl->yBottom = newY;

}

VOID initFrame( HWND hwndFrame )
{
    static BOOL fInitFirst = TRUE;

    HWND        hwndSysMenu;
    HWND        hwndClient;
    MENUITEM    mi;
    HPS         hps;
    SIZEF       sizef;
    FONTMETRICS fm;
    RECTL       rcl;

    if( fInitFirst )
    {
        hwndSysMenu = WinWindowFromID( hwndFrame, FID_SYSMENU );
        WinSendMsg( hwndSysMenu, MM_QUERYITEM, MPFROM2SHORT( SC_SYSMENU, FALSE ), ( MPARAM )&mi );

        hwndSysMenu = mi.hwndSubMenu;
        WinSendMsg( hwndSysMenu, MM_QUERYITEM, MPFROM2SHORT( SC_CLOSE, FALSE ), ( MPARAM )&mi );

        mi.iPosition += 2;
        mi.afStyle = MIS_TEXT;
        mi.afAttribute = 0;
        mi.id = IDM_CODEPAGE;
        mi.hwndSubMenu = NULLHANDLE;
        mi.hItem = 0;
        WinSendMsg( hwndSysMenu, MM_INSERTITEM, ( MPARAM )&mi, ( MPARAM )"Codepage..." );

        mi.iPosition++;
        mi.afStyle = MIS_TEXT;
        mi.afAttribute = 0;
        mi.id = IDM_FONT;
        mi.hwndSubMenu = NULLHANDLE;
        mi.hItem = 0;
        WinSendMsg( hwndSysMenu, MM_INSERTITEM, ( MPARAM )&mi, ( MPARAM )"Font..." );

        mi.iPosition++;
        mi.afStyle = MIS_SEPARATOR;
        mi.afAttribute = 0;
        mi.id = 0;
        mi.hwndSubMenu = NULLHANDLE;
        mi.hItem = 0;
        WinSendMsg( hwndSysMenu, MM_INSERTITEM, ( MPARAM )&mi, 0 );
    }

    hwndClient = WinWindowFromID( hwndFrame, FID_CLIENT );

    hps = WinGetPS( hwndClient );
    GpiCreateLogFont( hps, NULL, 1L, &m_fat );
    GpiSetCharSet( hps, 1L );

    sizef.cx = m_fxPointSize;
    sizef.cy = m_fxPointSize;

    GpiSetCharBox( hps, &sizef );

    GpiQueryFontMetrics( hps, sizeof( FONTMETRICS ), &fm );

    m_lCharWidth = fm.lAveCharWidth;
    m_lCharHeight = fm.lMaxBaselineExt + fm.lExternalLeading;
    m_lMaxDescender = fm.lMaxDescender;

    WinReleasePS( hps );

    WinQueryWindowRect( hwndClient, &rcl );
    WinMapWindowPoints( hwndClient, HWND_DESKTOP, ( PPOINTL )&rcl, 2 );
    rcl.xRight = rcl.xLeft + m_lCharWidth * m_vmi.col;
    rcl.yTop = rcl.yBottom + m_lCharHeight * m_vmi.row;
    WinCalcFrameRect( hwndFrame, &rcl, FALSE );

    if( fInitFirst )
    {
        randomizeRect( &rcl );
        fInitFirst = FALSE;
    }
    else
    {
        RECTL rclFrame;

        WinQueryWindowRect( hwndFrame, &rclFrame );
        WinMapWindowPoints( hwndFrame, HWND_DESKTOP, ( PPOINTL)&rclFrame, 2 );

        WinOffsetRect( WinQueryAnchorBlock( hwndFrame ), &rcl, 0, rclFrame.yTop - rcl.yTop );
    }

    WinSetWindowPos( hwndFrame, HWND_TOP,
                     rcl.xLeft, rcl.yBottom,
                     rcl.xRight - rcl.xLeft,
                     rcl.yTop - rcl.yBottom,
                     SWP_SIZE | SWP_MOVE | SWP_ZORDER | SWP_SHOW | SWP_ACTIVATE );
}

BOOL    callVioDmn( USHORT usMsg )
{
    static  BOOL fQuit = FALSE;

    USHORT  usAck;
    ULONG   cbActual;

    if( fQuit )
        return TRUE;

    do
    {
        DosCallNPipe(
                m_szPipeName,
                &usMsg, sizeof( usMsg ),
                &usAck, sizeof( usAck ), &cbActual,
                10000L );
        DosSleep( 1 );
    } while(( usAck != MSG_DONE ) && ( usAck != MSG_QUIT ));

    if( usAck == MSG_QUIT )
        fQuit = TRUE;

    return fQuit;
}

BOOL startVioDmn( VOID )
{
    STARTDATA   stdata;
    CHAR        szInput[ 256 ];
    CHAR        ObjectBuffer[ 256 ];

    strcpy( szInput, m_szPid );
    strcat( szInput, " " );
    strcat( szInput, VIODMN_MAGIC );

    stdata.Length = sizeof( stdata );
    stdata.Related = SSF_RELATED_INDEPENDENT;
    stdata.FgBg = SSF_FGBG_BACK;
    stdata.TraceOpt = SSF_TRACEOPT_NONE;
    stdata.PgmTitle = "VIO Daemon";
    stdata.PgmName = "VIODMN.EXE";
    stdata.PgmInputs = szInput;
    stdata.TermQ = 0;
    stdata.Environment = 0;
    stdata.InheritOpt = SSF_INHERTOPT_PARENT;
    stdata.SessionType = SSF_TYPE_FULLSCREEN;
    stdata.IconFile = 0;
    stdata.PgmHandle = 0;
    stdata.PgmControl = SSF_CONTROL_INVISIBLE;
    stdata.ObjectBuffer = ObjectBuffer;
    stdata.ObjectBuffLen = sizeof( ObjectBuffer );

    return ( DosStartSession( &stdata, &m_sidVioDmn, &m_pidVioDmn ) == 0 );
}

VOID waitVioDmn( VOID )
{
    while( DosWaitNPipe( m_szPipeName, ( ULONG )-1 ) == ERROR_INTERRUPT );
}


