#ifndef __VIODMN_H_
#define __VIODMN_H_

#define VIODMN_MAGIC    "VioDmn_should_be_run_by_KShell_!!!"

#define MSG_PIPE_SIZE   4   /* ULONG */

#define PIPE_NAME_LEN       64
#define PIPE_VIO_DUMP_BASE  "\\PIPE\\VIO\\DUMP\\"

#define MSG_DUMP    0x0001
#define MSG_CHAR    0x0002
#define MSG_VIOINFO 0x0003
#define MSG_QUIT    0x0013
#define MSG_DONE    0xFFFF

#endif

