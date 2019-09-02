                             KShell v0.8.1
                            ---------------
                            
1. Introduction
---------------
  
  This program is intended to help Input/Output of DBCS, especially Hangul, 
on VIO of non-DBCS OS/2 environment.

2. Development Environments  
---------------------------
  
  - OS/2 Warp v4 for Korean with FixPak #15
  
  - Open Watcom v1.8
  
3. Requirement
--------------

  - Appropriate fonts
  
  - IME for input DBCS, such as KIME

4. Test Environmet
------------------

  This program is tested on OS/2 Warp v4 for Korean with FixPak #15 and 
eComStation v1.2MR.

5. Features
-----------

  - Redirection of VIO output to PM window, hooking of OS/2 Base Video
    Subsystem, and transfer key sequences of PM window to VIO, using keyboard
    monitor.
    
  - Any fonts other than System font can be used on VIO. But only fixed width 
    fonts are allowed.
  
  - Codepages other than primary or secondary codepage can be specified.
  
  - Scroll-back up to 200 lines is supported.
  
  - Clipboard(unicode) is supported.
  
  - FT2LIB is supported.

6. Installation
---------------

  Put kshell.exe, viodmn.exe and viosub.dll into any directory.
  
7. Usage
--------

7-1. System Menu
----------------

  Codepage... : Specify codepage.
  Font...     : Specify font and size.

7-2. Popup Menu
---------------
  Codepage... : Specify codepage.
  Font...     : Specify font and size.
  Copy        : Copy the contents of marking area to clipboard.
  Copy All    : Copy all the contents of the window to clipboard.
  Paste       : Copy from clipboard to the window.
  Use FT2LIB  : Specify whether to use FT2LIB or not.
  
8. Limits/Known bugs
--------------------

  - Only support keyboard input and VIO output. So mouse is not supported.

  - Ctrl-S, Ctrl-P, and so on are not supported.
  
  - With Q.EXE, Shift-Arrow Keys does not work.
  
  - On FT2LIB mode, width of font can be too wide.
  
9. TODOs...
-----------

  - Support mouse
  
  - Support special key combination such as Ctrl-P.
  
10. History
---------------

  - V0.8.1 ( 2012/02/05 )
    .Performance sometimes becomes very poor. Fixed.
    
  - v0.8.0 ( 2010/12/26 )
    .viodmn : Support Pause key.
    .viodmn, ft2lib : Improved the compatibility with LIBPATHSTRICT=T
    .ft2lib : The return value is not defined if loading FT2LIB.DLL failed. 
              Fixed.
    .viodmn : Named shared memories are not freed correctly. Fixed.
    .Changed compiler to Open Watcom v1.8.
    
  - v0.7.0 ( 2008/10/05 )
    .Support FT2LIB.
    .Support icon( Contributed by Alex Taylor ).
    .Support unicode for clipboard( Contributed by Alex Taylor ).
    .Clipboard is not cleared when copying to clipboard
     ( Reported by Alex Taylor ).
    .Part of popup window can be out of screen. Fixed.

  - v0.6.0 ( 2007/04/07 )
    .Support VioSetMode(). So 43/50 line mode can be selected as well as 25 
     line mode.
    .Changing font cause codepage to be changed. Fixed.
    .Even though codepage is changed, DBCS initialization is not performed. 
     Fixed.
    .XCOPY.EXE conflict with debug build. Fixed( Disabled stack overflow check 
     with -s option )

  - v0.5.0 ( 2007/03/14 )
    .Support Ctrl-C and Ctrl-Break key combination.
    .Support KSHELL_COMSPEC env. var.
    .donePipeThreadForVioSub() call _endthread() incorrectly. Fixed.
    .On file list screen of HV.EXE with v0.4.0, blank rectangle is displayed. 
     Fixed.
    .KShell doesn't respond to user input when scrolling heavily such as 'dir 
     x:\/s'. Fixed.
    .KShell doesn't respond to user input if obscured fully due to the thing 
     like full-screen switching when scrolling. Fixed.
    .'Marking' mode does not work correctly with 'Scroll-back' mode. Fixed.
    .XCOPY.EXE does not work with v0.4.0. Fixed. (v0.4.0 is debug build. Debug 
     build has a problem, but not release build. Maybe compiler bug ?)
    
  - v0.4.0 ( 2007/02/26 )
    .'Courier New' font do not displayed correctly. Fixed.
    .When 'Marking Mode', if any window cross over the marking area, that 
     area is not displayed correctly. Fixed.
    .Changing compiler to OpenWatcom v1.6 since v0.3.0 cause vioGetBuf() 
     broken. Fixed.
    .With font association, a 'stair problem' occur. Fixed.
    .Changing scroll function cause scroll contents leak out of scroll 
     rectangle. Fixed.
    .Occasionally, CPU load is up to 99.9% and KShell hang. Fixed(I hope^^)
    
  - v0.3.0 ( 2007/01/01 )
    .Supports clipboard.
    .Minor bugs. Fixed.
    .Compiler changed to Open Watcom v1.6
    
  - v0.2.0 ( 2005/08/16 )
    .Improved performance greately.
    .Sometimes, a part of background is not erased. Fixed.
    .When scrolling, other window cover KShell window, that part of KShell
     window is not scrolled correctly. Fixed( using WinScrollWindow() instead 
     of GpiBitBlt()).
    .HyperView( HV.EXE ) cannot be launched. Fixed( enlarging buffer size for
     DosQuerySysState()).

  - v0.1.0 ( 2005/02/28 )
    .Use VioRegister() instead of polling method. As a result, Output 
     efficiency is improved.
    .VIO colors are correct.
    .Broken DBCS chars are displayed when inputting consecutive DBCS chars.
     Fixed.
    .If font size is not changed even though font face is changed, window is
     not updated. Fixed.
    .In case of bitmap font such as MINCHO, it is not retrieved on next time.
     Fixed.
    .The unit of font size is changed from pel to pt.

  - test version ( 2005/01/19 )
    .initial version.
    
11. Compilation
---------------
 
  You have to have Open Watcom v1.8 to compile KShell.
  Now, enter 'wmake' on command prompt.
  
12. Modules
-------------

  kshell.exe : PM program that display VIO buffer on PM and pass key on PM to 
               VIO.

  viodmn.exe : VIO full-screen program that launch a command shell specified 
               by 'KSHELL_COMSPEC', 'COMSPEC' and 'CMD.EXE', and interconnect 
               VIO and PM. 

               !!! viodmn.exe should be started by kshell.exe !!!

  viosub.dll : DLL to hook OS/2 Base Video Subsystem.

  test.exe   : Key test program on VIO.
  
13. Thanks to...
----------------

  nickk : provided me with examples for VioRegister().

14. Donation
------------

  If you are satisfied with this program and want to donate to me, please visit
the following URL.

    http://www.ecomstation.co.kr/komh/donate.html

15. Contact
-----------

  e-mail : komh@chollian.net
  ICQ    : 124861818
  MSN    : komh@chollian.net
  IRC    : lvzuufx, #os2 at HANIRC(irc.hanirc.org)
               
                                                       KO Myung-Hun
               
