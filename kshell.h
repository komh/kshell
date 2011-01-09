#ifndef __KSHELL_H_
#define __KSHELL_H_

#define INCL_VIO
#include <os2.h>

#include "cpdlg.h"

#define WC_KSHELL   "KShellClass"

#define ID_KSHELL   1

#define IDM_CODEPAGE    0x01
#define IDM_FONT        0x02

#define VIO_CISIZE      ( sizeof( USHORT ) * 2 + sizeof( VIOCURSORINFO ))
#define VIO_CELLSIZE    2
#define VIO_SCRSIZE     ( m_vmi.col * m_vmi.row * VIO_CELLSIZE )

#define KSHELL_BUFSIZE  ( VIO_CISIZE + VIO_SCRSIZE )

#define KSHELL_MEMNAME_LEN  64
#define KSHELL_VIOBUF_BASE  "\\SHAREMEM\\KSHELL\\VIOBUF\\"

#define PM_SC_LSHIFT    0x2A
#define PM_SC_RSHIFT    0x36
#define PM_SC_LCTRL     0x1D
#define PM_SC_RCTRL     0x5B
#define PM_SC_LALT      0x38
#define PM_SC_RALT      0x5E

#endif
