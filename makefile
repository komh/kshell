.SUFFIXES : .h .c .o .obj .exe .res .dlg .dll .map

CC = gcc
CFLAGS = -Wall -Zomf -O3
MTFLAGS = -Zmt
LFLAGS = -Zomf -Zmap -Zlinker /map
DLLFLAGS = -Zdll -Zsys -Zso

!ifdef RELEASE
MAKEPARAM = RELEASE=1
!else
DBGFLAGS = -g
!endif

RC = rc

ZIP = zip

DEL = del

.c.obj :
    $(CC) $(DBGFLAGS) $(CFLAGS) $(MTFLAGS) -o $@ -c $<

.dlg.res :
    $(RC) -r $< $@

all : viodmn.exe kshell.exe cpdlg.res viosub test.exe

kshell.exe : kshell.obj kshell.def cpdlg.res
    $(CC) $(DBGFLAGS) $(LFLAGS) $(MTFLAGS) -o $@ $** -lbsd

viodmn.exe : viodmn.obj viodmn.def
    $(CC) $(DBGFLAGS) $(LFLAGS) $(MTFLAGS) -o $@ $**

test.exe : test.obj test.def
    $(CC) $(DBGFLAGS) $(LFLAGS) -o $@ $**

cpdlg.res : cpdlg.h cpdlg.dlg

viosub :
    $(MAKE) /f viosub.mak $(MAKEPARAM)

kshell.obj : kshell.c kshell.h cpdlg.h viodmn.h viosub.h

viodmn.obj : viodmn.c kshell.h cpdlg.h viodmn.h

test.obj : test.c

dist : bin src
    $(ZIP) kshell$(VER) kshellsrc.zip
    -$(DEL) kshellsrc.zip

bin : kshell.exe viodmn.exe viosub.dll test.exe readme.txt readme.eng
    -$(DEL) kshell$(VER)
    $(ZIP) kshell$(VER) $**

src : kshell.c kshell.h kshell.def viodmn.c viodmn.h viodmn.def viosub.c viosub.h \
      dosqss.h vioroute.asm viosub.def viosub.mak test.c test.def cpdlg.dlg cpdlg.h \
      makefile
    -$(DEL) kshellsrc
    $(ZIP) kshellsrc $**

clean :
    -$(DEL) *.map
    -$(DEL) *.obj
    -$(DEL) *.dll
    -$(DEL) *.exe
    -$(DEL) *.res
    -$(DEL) *.zip

