from building import *
import rtconfig

cwd  = GetCurrentDir()

src         = []
CPPPATH     = []
LIBS        = []
LIBPATH     = []

#src
src += Glob('*.c')

group = DefineGroup('tcpdump', src, depend = ['PKG_NETUTILS_TCPDUMP'], CPPPATH = CPPPATH, LIBS = LIBS, LIBPATH = LIBPATH)

Return('group')