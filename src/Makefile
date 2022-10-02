CC = g++
AR = ar
RM = rm

CDEBUG = -g
LDEBUG = -g

INC_DIR = ../include
OBJ_DIR = ../obj
LIB_DIR = ../lib
BIN_DIR = ../bin

all: $(BIN_DIR)/CPrePro

SRC = \
CPrePro.cpp \

OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC))

CPRE_PRO_STD_DIRS = \
/usr/include/c++/8 \
/usr/include/x86_64-linux-gnu/c++/8 \
/usr/lib/gcc/x86_64-linux-gnu/8/include \
/usr/include/x86_64-linux-gnu \

CPPFLAGS = \
-std=c++17 \
-I$(INC_DIR) \
-I. \
-I../../CExpr/include \
-I../../CFile/include \
-I../../CMath/include \
-I../../CStrUtil/include \
-I../../CUtil/include \

LFLAGS = \
-L../../CExpr/lib \
-L../../CFile/lib \
-L../../CMath/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \

LIBS = \
-lCExpr \
-lCFile \
-lCMath \
-lCStrUtil \
-lCOS \

clean:
	$(RM) -f $(OBJ_DIR)/*.o
	$(RM) -f $(BIN_DIR)/CPrePro

$(OBJS): $(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $< -o $(OBJ_DIR)/$*.o $(CPPFLAGS)

.SUFFIXES: .cpp

$(BIN_DIR)/CPrePro: $(OBJS)
	$(CC) $(LDEBUG) -o $(BIN_DIR)/CPrePro $(OBJS) $(LFLAGS) $(LIBS)
