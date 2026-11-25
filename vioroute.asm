.386
        .MODEL  FLAT

        PUBLIC  VioRouter
        PUBLIC  _SkipFlag

        EXTRN   _Entry32Main:NEAR
        EXTRN   DosSelToFlat:NEAR

CODE16          SEGMENT WORD PUBLIC USE16 'CODE'
                ASSUME  CS:CODE16

        ALIGN   2

; 16-bit entry point
VioRouter:
        push    ds
        push    es
        jmp far ptr FLAT:Entry32bit

        ALIGN   2
Return16bit:
        pop es
        pop ds
        retf
CODE16          ENDS


CODE32          SEGMENT DWORD PUBLIC USE32 'CODE'
                ASSUME  CS:FLAT,DS:FLAT

        ALIGN   4

; 32-bit entry point
Entry32bit:
        mov ax,seg DATA32
        mov ds,eax              ; ds = FLAT
        mov es,eax              ; es = FLAT

        cmp dword ptr [_SkipFlag], 0
        jz  continue
        mov ax, -1
        jmp far ptr CODE16:Return16bit

continue:
; Run 32-bit code on the caller's stack, not on a private stack.
; If we are called within a 32-bit API thunked to 16-bit by DOSCALL1,
; the caller's stack is a ThunkTIB of DOSCALL1. Then any thunked API
; called from a private stack, such as DosClose(), switches to the top
; of the ThunkTIB again and overwrites the frame of the outer API.
        push ss                         ; save caller ss on its own stack

        mov ax,ss
        shl eax,16
        mov ax,sp                       ; 16:16 caller stack
        call    DosSelToFlat            ; eax = flat caller stack

        push ds
        pop ss                          ; }
        mov esp,eax                     ; } same stack, now flat

; on stack - caller ss
        push eax
        and dword ptr [esp],0FFFFh      ; 16:32 caller ss:sp for lss

        pushad                  ; save all regs

        lea eax,[esp+8*4+8]             ; skip pushad, ss:sp and ss
        push    eax             ; 32-bit flat old stack

; for now: ds=es=ss=FLAT DATA, esp=flat caller stack
; on stack - old 16-bit stack pointer.
; call pure 32-bit entry point
        call    _Entry32Main

; return to 16-bit code
Return32bit:
        add esp,4               ; C calling conv.

        mov [esp+7*4],eax           ; replace eax
        popad                   ; restore all regs

        lss esp,fword ptr [esp]         ; restore old ss:sp
        add esp,4                       ; remove caller ss
        jmp far ptr CODE16:Return16bit
CODE32          ENDS


DATA32      SEGMENT DWORD PUBLIC USE32 'DATA'

        ALIGN   4
_SkipFlag       DD  0

DATA32      ENDS


                END
