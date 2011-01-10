.SUFFIXES : .h .c .o .obj .exe .res .dlg .dll

CC = icc
AS = alp
LINKER = ilink
CFLAGS = /W3 /Ss+ /G4 /O+
LFLAGS = /NOE /Map
DLLFLAGS = /Ge-

!ifndef RELEASE
CDBGFLAGS = /Ti+
ADBGFLAGS = +Od
LDBGFLAGS = /DE
!endif

.asm.obj :
    $(AS) $(ADBGFLAGS) $< -Fo:$@

.c.obj :
    $(CC) $(CDBGFLAGS) $(CFLAGS) $(DLLFLAGS) /C+ /Fo$@ $<

viosub.dll : viosub.obj vioroute.obj viosub.def
    $(LINKER) $(LDBGFLAGS) $(LFLAGS) /OUT:$@ $**

viosub.obj : viosub.c dosqss.h viosub.h kshell.h cpdlg.h

vioroute.obj : vioroute.asm
