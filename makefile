.SUFFIXES : .h .c .o .obj .exe .res .dlg

CC = gcc
CFLAGS = -Wall -Zomf
MTFLAGS = -Zmt
DLLFLAGS = -Zdll -Zsys -Zso
DBGFLAGS = -g

RC = rc

ZIP = zip

DEL = del

all : viodmn.exe kshell.exe cpdlg.res test.exe

cpdlg.res : cpdlg.h cpdlg.dlg
    $(RC) -r cpdlg.dlg cpdlg.res

kshell.exe : kshell.c kshell.h viodmn.h kshell.def cpdlg.res
    $(CC) $(DBGFLAGS) $(CFLAGS) -o kshell.exe kshell.c kshell.def cpdlg.res -lbsd

viodmn.exe : viodmn.c kshell.h viodmn.h viodmn.def
    $(CC) $(DBGFLAGS) $(CFLAGS) $(MTFLAGS) -o viodmn.exe viodmn.c viodmn.def

test.exe : test.c test.def
    $(CC) $(DBGFLAGS) $(CFLAGS) -o test.exe test.c test.def

dist : bin src
    $(ZIP) kshell$(VER) kshellsrc.zip
    -$(DEL) kshellsrc.zip

bin :
    -$(DEL) kshell$(VER)
    $(ZIP) kshell$(VER) kshell.exe viodmn.exe test.exe readme.txt readme.eng

src :
    -$(DEL) kshellsrc
    $(ZIP) kshellsrc kshell.c kshell.h kshell.def test.c test.def viodmn.c viodmn.h \
           viodmn.def cpdlg.dlg cpdlg.h makefile

clean :
    -$(DEL) *.o
    -$(DEL) *.obj
    -$(DEL) *.dll
    -$(DEL) *.exe
    -$(DEL) *.res
    -$(DEL) *.zip

