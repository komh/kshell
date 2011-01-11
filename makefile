.ERASE

.SUFFIXES :
.SUFFIXES : .exe .dll .res .obj .h .asm .c .dlg .rc

AS = wasm
AFLAGS = -zq

CC = wcc386
CFLAGS = -zq -wx -we -bm

CL = wcl386

LINK = wlink
LFLAGS = option quiet

!ifdef RELEASE
CFLAGS += -d0 -oaxt
!else
CFLAGS += -d2
AFLAGS += -d2
LFLAGS += debug all
!endif

RC = wrc
RCFLAGS = -q

ZIP = zip

DEL = del

.asm.obj :
    $(AS) $(AFLAGS) -fo=$@ $[@

.c.obj :
    $(CC) $(CFLAGS) -fo=$@ $[@

.rc.res :
    $(RC) $(RCFLAGS) -r $[@ $@

all : viodmn.exe kshell.exe kshell.res viosub.dll test.exe

kshell.exe : kshell.obj kshell.res
    $(CL) $(CFLAGS) -l=os2v2_pm -fe=$@ $[@
    $(RC) $(RCFLAGS) $]@ -fe=$@

viodmn.exe : viodmn.obj
    $(CL) $(CFLAGS) -l=os2v2 -fe=$@ $<

test.exe : test.obj
    $(CL) $(CFLAGS) -l=os2v2 -fe=$@ $<

kshell.res : kshell.rc kshell.h cpdlg.h cpdlg.dlg

viosub.dll : viosub.obj vioroute.obj viosub.lnk
    $(LINK) $(LFLAGS) @viosub.lnk

kshell.obj : kshell.c kshell.h cpdlg.h viodmn.h viosub.h

viodmn.obj : viodmn.c kshell.h cpdlg.h viodmn.h

viosub.obj : viosub.c dosqss.h viosub.h kshell.h cpdlg.h

vioroute.obj : vioroute.asm

test.obj : test.c

dist : .SYMBOLIC bin src
    $(ZIP) kshell$(VER) kshellsrc.zip
    -$(DEL) kshellsrc.zip

bin : kshell.exe viodmn.exe viosub.dll test.exe readme.txt readme.eng .SYMBOLIC
    -$(DEL) kshell$(VER).zip
    $(ZIP) kshell$(VER) $<

src : .SYMBOLIC kshell.c kshell.h kshell.rc &
      viodmn.c viodmn.h viosub.c viosub.h vioroute.asm viosub.lnk &
      dosqss.h test.c cpdlg.dlg cpdlg.h &
      makefile .SYMBOLIC
    -$(DEL) kshellsrc.zip
    $(ZIP) kshellsrc $<

clean : .SYMBOLIC
    -$(DEL) *.map
    -$(DEL) *.obj
    -$(DEL) *.dll
    -$(DEL) *.exe
    -$(DEL) *.res
    -$(DEL) *.zip

