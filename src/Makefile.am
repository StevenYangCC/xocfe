TOP_DIR=..

INCLUDES = -I./ \
           -I./com/ \
           -I./cfe/ \
           -I./cfe.prj/ \
           -I./opt/

VPATH = ./cfe ./com ./opt ./cfe.prj

bin_PROGRAMS = xocfe

AM_CFLAGS = -Wno-write-strings -Wsign-promo \
        -Wsign-compare -Wpointer-arith -Wno-multichar -Winit-self \
        -Wstrict-aliasing=3 -D_DEBUG_ -Wswitch
AM_CXXFLAGS = $(AM_CFLAGS) -fno-rtti -fno-exceptions

xocfe_SOURCES = \
                cfe.prj/xocfe.cpp \
                cfe/decl.cpp \
                cfe/err.cpp \
                cfe/exectree.cpp \
                cfe/lex.cpp \
                cfe/scope.cpp \
                cfe/st.cpp \
                cfe/tree.cpp \
                cfe/treegen.cpp \
                cfe/typeck.cpp \
                cfe/cell.cpp \
                \
                com/smempool.cpp \
                com/comf.cpp \
                com/bs.cpp \
                com/ltype.cpp \
                com/strbuf.cpp \
                \
                opt/label.cpp \
                opt/util.cpp


