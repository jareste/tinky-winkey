cl /Wall /WX /wd4668 tinky.c /link /out:tinky.exe Advapi32.lib

sc start tinky

sc stop tinky

sc delete tinky

sc query tinky
