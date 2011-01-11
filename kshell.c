#define INCL_VIO
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DEV
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kshell.h"
#include "viodmn.h"
#include "viosub.h"

#define TID_KSHELL  ( TID_USERMAX - 1 )

#define PRF_APP         "KSHELL"
#define PRF_KEY_CP      "CODEPAGE"
#define PRF_KEY_FONT    "FONT"
#define PRF_KEY_SIZE    "SIZE"
#define PRF_KEY_HEIGHT  "HEIGHT"
#define PRF_KEY_WIDTH   "WIDTH"

#define DEFAULT_CODEPAGE    0
#define DEFAULT_FONT_FACE   "GulimChe"
#define DEFAULT_CHAR_PTS    12
#define DEFAULT_CHAR_HEIGHT 16
#define DEFAULT_CHAR_WIDTH  8

#define BASE_TITLE      "KShell"

static VIOMODEINFO  m_vmi;

static FATTRS   m_fat;
static FIXED    m_fxPointSize;
static ULONG    m_ulHoriFontRes;
static ULONG    m_ulVertFontRes;
static ULONG    m_lCharWidth;
static ULONG    m_lCharHeight;
static ULONG    m_lMaxDescender;

static PBYTE    m_pVioBuf = NULL;
static CHAR     m_szPipeName[ PIPE_VIODMN_LEN ];
static CHAR     m_szPid[ 20 ];

static HEV      m_hevVioDmn;

static PID      m_pidVioDmn;
static ULONG    m_sidVioDmn;

static BOOL     m_afDBCSLeadByte[ 256 ] = { FALSE, };
static BOOL     m_fDBCSEnv = FALSE;

#define isDBCSEnv() ( m_fDBCSEnv )

static ULONG    m_ulSGID = ( ULONG )-1;
static HPIPE    m_hpipeVioSub = NULLHANDLE;
static TID      m_tidPipeThread = 0;

static BOOL init( VOID );
static VOID done( VOID );

static VOID initDBCSEnv( USHORT usCP );
static VOID initFrame( HWND hwndFrame );

#define     isDBCSLeadByte( uch ) ( m_afDBCSLeadByte[( uch )])

static BOOL callVioDmn( USHORT usMsg );

static BOOL startVioDmn( VOID );
static VOID waitVioDmn( VOID );

static VOID initPipeThreadForVioSub( HWND hwnd );
static VOID donePipeThreadForVioSub( VOID );

static VOID updateWindow( HWND hwnd, PRECTL prcl );

static MRESULT EXPENTRY windowProc( HWND, ULONG, MPARAM, MPARAM );

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

    if( callVioDmn( MSG_SGID ))
    {
        result = 3;
        goto main_exit;
    }

    memcpy( &m_ulSGID, m_pVioBuf, sizeof( ULONG ));

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
                BASE_TITLE,                 // window title
                0L,                         // default client style
                NULLHANDLE,                 // resource in exe file
                ID_KSHELL,                  // frame window id
                &hwndClient                 // client window handle
                );

    // assume not failing
    initPipeThreadForVioSub( hwndClient );

    initFrame( hwndFrame );

    while( WinGetMsg( hab, &qm, NULLHANDLE, 0, 0 ))
        WinDispatchMsg( hab, &qm );

    donePipeThreadForVioSub();

    WinDestroyWindow( hwndFrame );

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

    if(( pKShellData->ci.attr != ( USHORT )-1 ) &&
       ( WinQueryFocus( HWND_DESKTOP ) == hwnd ) &&
       fCreate )
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

static VOID setAttr( HPS hps, UCHAR uchAttr )
{
    static int aiColorTable[ 16 ] = {
            CLR_BLACK,
            CLR_DARKBLUE,
            CLR_DARKGREEN,
            CLR_DARKCYAN,
            CLR_DARKRED,
            CLR_DARKPINK,
            CLR_BROWN,
            CLR_PALEGRAY,
            CLR_DARKGRAY,
            CLR_BLUE,
            CLR_GREEN,
            CLR_CYAN,
            CLR_RED,
            CLR_PINK,
            CLR_YELLOW,
            CLR_WHITE
    };

    GpiSetColor( hps, aiColorTable[ uchAttr & 0x0F ]);
    GpiSetBackColor( hps, aiColorTable[ ( uchAttr & 0xF0 ) >> 4 ]);
}

VOID updateWindow( HWND hwnd, PRECTL prcl )
{
    PKSHELLDATA pKShellData = WinQueryWindowPtr( hwnd, 0 );

    HPS     hps;
    SIZEF   sizef;
    int     xStart, yStart;
    int     xEnd, yEnd;

    setCursor( hwnd, FALSE );

    hps = WinGetPS( hwnd );

    xStart = X_Win2Vio( prcl->xLeft );
    yStart = Y_Win2Vio( prcl->yTop - 1 );
    xEnd = X_Win2Vio( prcl->xRight - 1 );
    yEnd = Y_Win2Vio( prcl->yBottom );

    GpiCreateLogFont( hps, NULL, 1L, &m_fat );

    GpiSetCharSet( hps, 1L );

    sizef.cx = m_fxPointSize * m_ulHoriFontRes / 72;
    sizef.cy = m_fxPointSize * m_ulVertFontRes / 72;

    GpiSetCharBox( hps, &sizef );
    GpiSetBackMix( hps, BM_OVERPAINT );

    {
        int     x, y;
        POINTL  ptl;
        PUSHORT pVioBufShell;
        USHORT  usChar;
        USHORT  usLen;

        ptl.y = ( m_vmi.row - yStart - 1 ) * m_lCharHeight + m_lMaxDescender;
        for( y = yStart; y <= yEnd; y++ )
        {
            pVioBufShell = (( PUSHORT )pKShellData->pVioBuf )
                            + ( y * m_vmi.col );
            if( isDBCSEnv())
            {

                for( x = 0; x < xStart; x++, pVioBufShell++ )
                {
                    if( isDBCSLeadByte( LOUCHAR( *pVioBufShell )))
                    {
                        x++;
                        pVioBufShell++;
                    }
                }

                if( xStart < x ) // dbcs trail byte ?
                {
                    // to dbcs lead byte
                    xStart--;
                    pVioBufShell -= VIO_CELLSIZE;
                }
            }
            else
                pVioBufShell += xStart;

            ptl.x = xStart * m_lCharWidth;

            for( x = xStart; x <= xEnd; x++ )
            {
                setAttr( hps, HIUCHAR( *pVioBufShell ) );

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

    WinReleasePS( hps );

    setCursor( hwnd, TRUE );

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

            WinSetWindowPtr( hwnd, 0, pKShellData );

            pKShellData->pVioBuf = malloc( VIO_SCRSIZE );
            memset( pKShellData->pVioBuf, 0, sizeof( VIO_SCRSIZE ));

            if( callVioDmn( MSG_CURINFO ))
            {
                WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                return 0;
            }

            memcpy( pKShellData, m_pVioBuf, VIO_CISIZE );

            return 0;
        }

        case WM_DESTROY :
        {
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
            HPS     hps;
            RECTL   rcl;

            hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl );

            if( !WinIsRectEmpty( WinQueryAnchorBlock( hwnd ), &rcl ))
                updateWindow( hwnd, &rcl );

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
                        CHAR    szNum[ 10 ];

                        memcpy( &m_fat, &fd.fAttrs, sizeof( FATTRS ));
                        m_fxPointSize = fd.fxPointSize;

                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_FONT, m_fat.szFacename );

                        _itoa( FIXEDINT( m_fxPointSize ), szNum, 10 );
                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_SIZE, szNum );

                        _ltoa( m_fat.lMaxBaselineExt , szNum, 10 );
                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_HEIGHT, szNum );

                        _ltoa( m_fat.lAveCharWidth, szNum, 10 );
                        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_WIDTH, szNum );

                        initFrame( WinQueryWindow( hwnd, QW_PARENT ));
                        WinInvalidateRect( hwnd, NULL, FALSE );
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
    CHAR    szSem[ SEM_KSHELL_VIODMN_LEN ];
    CHAR    szMem[ MEM_KSHELL_VIOBUF_LEN ];
    LONG    lResult;
    HPS     hps;
    HDC     hdc;

    DosGetInfoBlocks( NULL, &ppib );

    _ultoa( ppib->pib_ulpid, m_szPid, 16 );

    strcpy( szMem, MEM_KSHELL_VIOBUF_BASE );
    strcat( szMem, m_szPid );

    DosAllocSharedMem(( PPVOID )&m_pVioBuf, szMem, KSHELL_BUFSIZE,
                      PAG_READ | PAG_WRITE | PAG_COMMIT );

    strcpy( m_szPipeName, PIPE_VIODMN_BASE );
    strcat( m_szPipeName, m_szPid );

    strcpy( szSem, SEM_KSHELL_VIODMN_BASE );
    strcat( szSem, m_szPid );

    DosCreateEventSem( szSem, &m_hevVioDmn, DC_SEM_SHARED, 0 );

    srandom( time( NULL ));

    PrfQueryProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_FONT, DEFAULT_FONT_FACE, m_fat.szFacename, FACESIZE );

    m_fat.usRecordLength = sizeof( FATTRS );
    m_fat.fsSelection = 0;
    m_fat.lMatch = 0L;
    m_fat.idRegistry = 0;
    m_fat.usCodePage = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_CP, DEFAULT_CODEPAGE );

    lResult = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_HEIGHT, DEFAULT_CHAR_HEIGHT );
    m_fat.lMaxBaselineExt = lResult ? lResult : DEFAULT_CHAR_HEIGHT;

    lResult = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_WIDTH, DEFAULT_CHAR_WIDTH );
    m_fat.lAveCharWidth = lResult ? lResult : DEFAULT_CHAR_WIDTH;

    m_fat.fsType = FATTR_TYPE_MBCS | FATTR_TYPE_DBCS;
    m_fat.fsFontUse = FATTR_FONTUSE_NOMIX;

    lResult = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_SIZE, DEFAULT_CHAR_PTS );
    m_fxPointSize = MAKEFIXED( lResult ? lResult : DEFAULT_CHAR_PTS, 0 );

    initDBCSEnv( m_fat.usCodePage );

    hps = WinGetScreenPS( HWND_DESKTOP );
    hdc = GpiQueryDevice( hps );
    DevQueryCaps( hdc, CAPS_HORIZONTAL_FONT_RES, 1, &m_ulHoriFontRes );
    DevQueryCaps( hdc, CAPS_VERTICAL_FONT_RES, 1, &m_ulVertFontRes );
    WinReleasePS( hps );

    return TRUE;
}

VOID done( VOID )
{
    DosCloseEventSem( m_hevVioDmn );

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
            {
                m_afDBCSLeadByte[ j ] = TRUE;
                m_fDBCSEnv = TRUE;
            }
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

    sizef.cx = m_fxPointSize * m_ulHoriFontRes / 72;
    sizef.cy = m_fxPointSize * m_ulVertFontRes / 72;

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

    while( DosCallNPipe( m_szPipeName,
                      &usMsg, sizeof( usMsg ),
                      &usAck, sizeof( usAck ), &cbActual,
                      10000L ) == ERROR_INTERRUPT );

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
    while( DosWaitEventSem( m_hevVioDmn, SEM_INDEFINITE_WAIT ) == ERROR_INTERRUPT );
}

static VOID pipeThread( void *arg )
{
    HWND        hwnd = ( HWND )arg;
    PKSHELLDATA pKShellData = WinQueryWindowPtr( hwnd, 0 );

    USHORT usIndex;
    ULONG  cbActual;

    do
    {
        DosConnectNPipe( m_hpipeVioSub );

        DosRead( m_hpipeVioSub, &usIndex, sizeof( USHORT ), &cbActual );

        switch( usIndex )
        {
            //case VI_VIOSHOWBUF :
            case VI_VIOSETCURPOS :
            {
                DosRead( m_hpipeVioSub, &pKShellData->x, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &pKShellData->y, sizeof( USHORT ), &cbActual );

                setCursor( hwnd, TRUE );
                break;
            }

            case VI_VIOSETCURTYPE :
            {
                VIOCURSORINFO vci;
                USHORT usCellHeight = m_vmi.vres / m_vmi.row;

                DosRead( m_hpipeVioSub, &vci, sizeof( VIOCURSORINFO ), &cbActual );

                if(( SHORT )vci.yStart < 0 )
                    vci.yStart = ( usCellHeight * ( -( SHORT )vci.yStart ) + 99 ) / 100 - 1;

                if(( SHORT )vci.cEnd < 0 )
                    vci.cEnd = ( usCellHeight * ( -( SHORT )vci.cEnd ) + 99 ) / 100 - 1;

                if( vci.yStart >= usCellHeight )
                    vci.yStart = usCellHeight - 1;

                if( vci.cEnd > 31 )
                    vci.cEnd = 31;

                memcpy( &pKShellData->ci, &vci, sizeof( VIOCURSORINFO ));

                setCursor( hwnd, TRUE );
                break;
            }

            //case VI_VIOSETMODE :
            case VI_VIOWRTNCHAR :
            {
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usTimes;
                CHAR    ch;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTimes, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &ch, sizeof( CHAR ), &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                for( y = usRow; ( y < m_vmi.row ) && ( usTimes > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usTimes > 0 ); x++, usTimes--, pBuf++ )
                    {
                       *pBuf = MAKEUSHORT( ch, HIUCHAR( *pBuf ));
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                break;
            }

            case VI_VIOWRTNATTR :
            {
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usTimes;
                BYTE    bAttr;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTimes, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &bAttr, sizeof( BYTE ), &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                for( y = usRow; ( y < m_vmi.row ) && ( usTimes > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usTimes > 0 ); x++, usTimes--, pBuf++ )
                    {
                       *pBuf = MAKEUSHORT( LOUCHAR( *pBuf ), bAttr );
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                break;
            }

            case VI_VIOWRTNCELL :
            {
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usTimes;
                USHORT  usCell;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTimes, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usCell, sizeof( BYTE ) * VIO_CELLSIZE, &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                for( y = usRow; ( y < m_vmi.row ) && ( usTimes > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usTimes > 0 ); x++, usTimes-- )
                    {
                       *pBuf++ = usCell;
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                break;
            }

            case VI_VIOWRTCHARSTR :
            {
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usLen;
                PCH     pchCharStr, pch;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLen, sizeof( USHORT ), &cbActual );

                pchCharStr = malloc( usLen );
                DosRead( m_hpipeVioSub, pchCharStr, usLen, &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                pch = pchCharStr;
                for( y = usRow; ( y < m_vmi.row ) && ( usLen > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usLen > 0 ); x++, usLen--, pBuf++ )
                    {
                       *pBuf = MAKEUSHORT( *pch++, HIUCHAR( *pBuf ));
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                free( pchCharStr );
                break;
            }

            case VI_VIOWRTCHARSTRATT :
            {
                BYTE    bAttr;
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usLen;
                PCH     pchCharStr, pch;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &bAttr, sizeof( BYTE ), &cbActual );
                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLen, sizeof( USHORT ), &cbActual );

                pchCharStr = malloc( usLen );
                DosRead( m_hpipeVioSub, pchCharStr, usLen, &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                pch = pchCharStr;
                for( y = usRow; ( y < m_vmi.row ) && ( usLen > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usLen > 0 ); x++, usLen-- )
                    {
                       *pBuf++ = MAKEUSHORT( *pch++, bAttr );
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                free( pchCharStr );
                break;
            }

            case VI_VIOWRTCELLSTR :
            {
                USHORT  usCol;
                USHORT  usRow;
                USHORT  usLen;
                PUSHORT pusCellStr, pusCell;
                PUSHORT pBuf;
                INT     x, y;
                RECTL   rcl;

                DosRead( m_hpipeVioSub, &usCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLen, sizeof( USHORT ), &cbActual );

                pusCellStr = malloc( usLen * sizeof( BYTE ) * VIO_CELLSIZE );
                DosRead( m_hpipeVioSub, pusCellStr, usLen * sizeof( BYTE ) * VIO_CELLSIZE , &cbActual );

                pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( usRow * m_vmi.col ) + usCol;
                pusCell = pusCellStr;
                for( y = usRow; ( y < m_vmi.row ) && ( usLen > 0 ); y++ )
                {
                    for( x = usCol; ( x < m_vmi.col ) && ( usLen > 0 ); x++, usLen-- )
                    {
                       *pBuf++ = *pusCell++;
                    }

                    rcl.xLeft = usCol;
                    rcl.yBottom = rcl.yTop = y;
                    rcl.xRight = x - 1;
                    convertVio2Win( &rcl );
                    updateWindow( hwnd, &rcl );

                    usCol = 0;
                }
                free( pusCellStr );
                break;
            }

            case VI_VIOSCROLLUP :
            {
                USHORT  usCell;
                USHORT  usLines;
                USHORT  usRightCol;
                USHORT  usBottomRow;
                USHORT  usLeftCol;
                USHORT  usTopRow;

                RECTL   rcl;

                PUSHORT pBuf;
                INT     x, y;

                DosRead( m_hpipeVioSub, &usCell, sizeof( BYTE ) * 2, &cbActual );
                DosRead( m_hpipeVioSub, &usLines, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRightCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usBottomRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLeftCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTopRow, sizeof( USHORT ), &cbActual );

                if( usLeftCol >= m_vmi.col )
                    usLeftCol = m_vmi.col - 1;

                if( usRightCol >= m_vmi.col )
                    usRightCol = m_vmi.col - 1;

                if( usBottomRow >= m_vmi.row )
                    usBottomRow = m_vmi.row - 1;

                if( usTopRow >= m_vmi.row )
                    usTopRow = m_vmi.row - 1;

                rcl.xLeft = usLeftCol;
                rcl.yBottom = usBottomRow;
                rcl.xRight = usRightCol;
                rcl.yTop = usTopRow;
                convertVio2Win( &rcl );

                if( usLines >= ( usBottomRow - usTopRow + 1 ))
                    usLines = usBottomRow - usTopRow + 1;
                else
                {
                    HPS     hps;
                    POINTL  aptl[ 3 ];

                    // source
                    aptl[ 2 ].x = rcl.xLeft;
                    aptl[ 2 ].y = rcl.yBottom;

                    // target
                    aptl[ 0 ].x = rcl.xLeft;
                    aptl[ 0 ].y = rcl.yBottom + usLines * m_lCharHeight;
                    aptl[ 1 ].x = rcl.xRight;
                    aptl[ 1 ].y = rcl.yTop;

                    setCursor( hwnd, FALSE );

                    hps = WinGetPS( hwnd );
                    GpiBitBlt( hps, hps, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );
                    WinReleasePS( hps );

                    setCursor( hwnd, TRUE );

                    for( y = usTopRow; y <= usBottomRow - usLines; y++ )
                    {
                        pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                        memcpy( pBuf, pBuf + ( usLines * m_vmi.col ), ( usRightCol - usLeftCol + 1 ) * VIO_CELLSIZE );
                    }
                }

                for( y = usBottomRow - usLines + 1; y <= usBottomRow; y++ )
                {
                    pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                    for( x = usLeftCol; x <= usRightCol; x++ )
                        *pBuf++ = usCell;
                }

                rcl.yTop = rcl.yBottom + usLines * m_lCharHeight;
                updateWindow( hwnd, &rcl );
                break;
            }

            case VI_VIOSCROLLDN :
            {
                USHORT  usCell;
                USHORT  usLines;
                USHORT  usRightCol;
                USHORT  usBottomRow;
                USHORT  usLeftCol;
                USHORT  usTopRow;

                RECTL   rcl;

                PUSHORT pBuf;
                INT     x, y;

                DosRead( m_hpipeVioSub, &usCell, sizeof( BYTE ) * 2, &cbActual );
                DosRead( m_hpipeVioSub, &usLines, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRightCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usBottomRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLeftCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTopRow, sizeof( USHORT ), &cbActual );

                if( usLeftCol >= m_vmi.col )
                    usLeftCol = m_vmi.col - 1;

                if( usRightCol >= m_vmi.col )
                    usRightCol = m_vmi.col - 1;

                if( usBottomRow >= m_vmi.row )
                    usBottomRow = m_vmi.row - 1;

                if( usTopRow >= m_vmi.row )
                    usTopRow = m_vmi.row - 1;

                rcl.xLeft = usLeftCol;
                rcl.yBottom = usBottomRow;
                rcl.xRight = usRightCol;
                rcl.yTop = usTopRow;
                convertVio2Win( &rcl );

                if( usLines >= ( usBottomRow - usTopRow + 1 ))
                    usLines = usBottomRow - usTopRow + 1;
                else
                {
                    HPS     hps;
                    POINTL  aptl[ 3 ];

                    // source
                    aptl[ 2 ].x = rcl.xLeft;
                    aptl[ 2 ].y = rcl.yBottom + usLines * m_lCharHeight;

                    // target
                    aptl[ 0 ].x = rcl.xLeft;
                    aptl[ 0 ].y = rcl.yBottom;
                    aptl[ 1 ].x = rcl.xRight;
                    aptl[ 1 ].y = rcl.yTop - usLines * m_lCharHeight;

                    setCursor( hwnd, FALSE );

                    hps = WinGetPS( hwnd );
                    GpiBitBlt( hps, hps, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );
                    WinReleasePS( hps );

                    setCursor( hwnd, TRUE );

                    for( y = usBottomRow; y >= usTopRow + usLines; y-- )
                    {
                        pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                        memcpy( pBuf, pBuf - ( usLines * m_vmi.col ), ( usRightCol - usLeftCol + 1 ) * VIO_CELLSIZE );
                    }
                }

                for( y = usTopRow; y < usTopRow + usLines; y++ )
                {
                    pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                    for( x = usLeftCol; x <= usRightCol; x++ )
                        *pBuf++ = usCell;
                }

                rcl.yBottom = rcl.yTop - usLines * m_lCharHeight;
                updateWindow( hwnd, &rcl );
                break;
            }

            case VI_VIOSCROLLLF :
            {
                USHORT  usCell;
                USHORT  usLines;
                USHORT  usRightCol;
                USHORT  usBottomRow;
                USHORT  usLeftCol;
                USHORT  usTopRow;

                RECTL   rcl;

                PUSHORT pBuf;
                INT     x, y;

                DosRead( m_hpipeVioSub, &usCell, sizeof( BYTE ) * 2, &cbActual );
                DosRead( m_hpipeVioSub, &usLines, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRightCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usBottomRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLeftCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTopRow, sizeof( USHORT ), &cbActual );

                if( usLeftCol >= m_vmi.col )
                    usLeftCol = m_vmi.col - 1;

                if( usRightCol >= m_vmi.col )
                    usRightCol = m_vmi.col - 1;

                if( usBottomRow >= m_vmi.row )
                    usBottomRow = m_vmi.row - 1;

                if( usTopRow >= m_vmi.row )
                    usTopRow = m_vmi.row - 1;

                rcl.xLeft = usLeftCol;
                rcl.yBottom = usBottomRow;
                rcl.xRight = usRightCol;
                rcl.yTop = usTopRow;
                convertVio2Win( &rcl );

                if( usLines >= ( usBottomRow - usTopRow + 1 ))
                    usLines = usBottomRow - usTopRow + 1;
                else
                {
                    HPS     hps;
                    POINTL  aptl[ 3 ];

                    // source
                    aptl[ 2 ].x = rcl.xLeft + usLines * m_lCharWidth;
                    aptl[ 2 ].y = rcl.yBottom;

                    // target
                    aptl[ 0 ].x = rcl.xLeft;
                    aptl[ 0 ].y = rcl.yBottom;
                    aptl[ 1 ].x = rcl.xRight - usLines * m_lCharWidth;
                    aptl[ 1 ].y = rcl.yTop;

                    setCursor( hwnd, FALSE );

                    hps = WinGetPS( hwnd );
                    GpiBitBlt( hps, hps, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );
                    WinReleasePS( hps );

                    setCursor( hwnd, TRUE );

                    for( y = usTopRow; y <= usBottomRow; y++ )
                    {
                        pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                        memmove( pBuf, pBuf + usLines, ( usRightCol - usLeftCol + 1 - usLines ) * VIO_CELLSIZE );
                    }
                }

                for( y = usTopRow; y <= usBottomRow; y++ )
                {
                    pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usRightCol - usLines + 1;
                    for( x = usRightCol - usLines + 1; x <= usRightCol; x++ )
                        *pBuf++ = usCell;
                }

                rcl.xLeft = rcl.xRight - usLines * m_lCharWidth;
                updateWindow( hwnd, &rcl );
                break;
            }

            case VI_VIOSCROLLRT :
            {
                USHORT  usCell;
                USHORT  usLines;
                USHORT  usRightCol;
                USHORT  usBottomRow;
                USHORT  usLeftCol;
                USHORT  usTopRow;

                RECTL   rcl;

                PUSHORT pBuf;
                INT     x, y;

                DosRead( m_hpipeVioSub, &usCell, sizeof( BYTE ) * 2, &cbActual );
                DosRead( m_hpipeVioSub, &usLines, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usRightCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usBottomRow, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usLeftCol, sizeof( USHORT ), &cbActual );
                DosRead( m_hpipeVioSub, &usTopRow, sizeof( USHORT ), &cbActual );

                if( usLeftCol >= m_vmi.col )
                    usLeftCol = m_vmi.col - 1;

                if( usRightCol >= m_vmi.col )
                    usRightCol = m_vmi.col - 1;

                if( usBottomRow >= m_vmi.row )
                    usBottomRow = m_vmi.row - 1;

                if( usTopRow >= m_vmi.row )
                    usTopRow = m_vmi.row - 1;

                rcl.xLeft = usLeftCol;
                rcl.yBottom = usBottomRow;
                rcl.xRight = usRightCol;
                rcl.yTop = usTopRow;
                convertVio2Win( &rcl );

                if( usLines >= ( usBottomRow - usTopRow + 1 ))
                    usLines = usBottomRow - usTopRow + 1;
                else
                {
                    HPS     hps;
                    POINTL  aptl[ 3 ];

                    // source
                    aptl[ 2 ].x = rcl.xLeft;
                    aptl[ 2 ].y = rcl.yBottom;

                    // target
                    aptl[ 0 ].x = rcl.xLeft + usLines * m_lCharWidth;
                    aptl[ 0 ].y = rcl.yBottom;
                    aptl[ 1 ].x = rcl.xRight;
                    aptl[ 1 ].y = rcl.yTop;

                    setCursor( hwnd, FALSE );

                    hps = WinGetPS( hwnd );
                    GpiBitBlt( hps, hps, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );
                    WinReleasePS( hps );

                    setCursor( hwnd, TRUE );

                    for( y = usTopRow; y <= usBottomRow; y++ )
                    {
                        pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                        memmove( pBuf + usLines, pBuf, ( usRightCol - usLeftCol + 1 - usLines ) * VIO_CELLSIZE );
                    }
                }

                for( y = usTopRow; y <= usBottomRow; y++ )
                {
                    pBuf = (( PUSHORT )pKShellData->pVioBuf ) + ( y * m_vmi.col ) + usLeftCol;
                    for( x = usLeftCol; x < usLeftCol + usLines; x++ )
                        *pBuf++ = usCell;
                }

                rcl.xRight = rcl.xLeft + usLines * m_lCharWidth;
                updateWindow( hwnd, &rcl );
                break;
            }
            //case VI_VIOPOPUP :
            //case VI_VIOENDPOPUP :

        }

        DosDisConnectNPipe( m_hpipeVioSub );
    } while( usIndex != ( USHORT )-1 );

    callVioDmn( MSG_QUIT );
    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
}

VOID initPipeThreadForVioSub( HWND hwnd )
{
    CHAR szName[ PIPE_KSHELL_VIOSUB_LEN ];
    CHAR szSem[ SEM_VIODMN_KSHELL_LEN ];
    HEV  hev = 0;

    ULONG rc;

    strcpy( szName, PIPE_KSHELL_VIOSUB_BASE );
    _ultoa( m_ulSGID, szName + strlen( szName ), 16 );

    rc = DosCreateNPipe( szName,
                    &m_hpipeVioSub,
                    NP_ACCESS_DUPLEX,
                    NP_WAIT | NP_TYPE_MESSAGE | NP_READMODE_MESSAGE | 0x01,
                    32768,
                    32768,
                    0 );

    m_tidPipeThread = _beginthread( pipeThread, NULL, 32768, ( void * )hwnd );

    strcpy( szSem, SEM_VIODMN_KSHELL_BASE );
    strcat( szSem, m_szPid );

    DosOpenEventSem( szSem, &hev );
    DosPostEventSem( hev );
    DosCloseEventSem( hev );
}

VOID donePipeThreadForVioSub( VOID )
{
    while( DosWaitThread( &m_tidPipeThread, DCWW_WAIT ) == ERROR_INTERRUPT );

    DosClose( m_hpipeVioSub );
}
