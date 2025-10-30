from building import *

cwd     = GetCurrentDir()
src     = Glob('*.c')

# 扫描当前目录下的所有子目录，并收集其中的 .c 文件
import os
subdirs = [d for d in os.listdir(cwd) if os.path.isdir(os.path.join(cwd, d))]
for subdir in subdirs:
    sub_src = Glob(os.path.join(subdir, '*.c'))
    src += sub_src

CPPPATH = [cwd] + [os.path.join(cwd, d) for d in subdirs]

group = DefineGroup('VIRTUAL_TERMINAL', src, depend = [], CPPPATH = CPPPATH)

Return('group')