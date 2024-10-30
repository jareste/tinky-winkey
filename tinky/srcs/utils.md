cl /Wall /WX /wd4668 /wd4820 tinky.c /link /out:tinky.exe Advapi32.lib Userenv.lib

cl /Wall /WX /wd4668 winkey.c /link /out:winkey.exe User32.lib Kernel32.lib 


sc start tinky

sc stop tinky

sc delete tinky

sc query tinky
