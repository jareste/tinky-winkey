NAME = winkey

SRC = srcs/winkey.c
OBJ_DIR = objs
OBJ = $(OBJ_DIR)\winkey.obj
INC_DIR = inc

# Compiler and flags
CC = cl
CFLAGS = /Wall /wd4668 /I$(INC_DIR) /D_WIN32_WINNT=0x0A00
LDFLAGS = User32.lib Kernel32.lib  Shell32.lib Ole32.lib Advapi32.lib /link

# Targets
all: $(NAME)

$(OBJ_DIR)\winkey.obj: $(SRC)
    if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
    $(CC) $(CFLAGS) /c $(SRC) /Fo$(OBJ)

$(NAME): $(OBJ)
    $(CC) $(OBJ) $(LDFLAGS) /OUT:$(NAME).exe
    @echo "Build completed successfully."

clean:
    if exist $(OBJ_DIR) rd /s /q $(OBJ_DIR)
    @echo "Clean completed."

fclean: clean
	if exist $(NAME).exe del $(NAME).exe
	@echo "Fclean completed."

re: fclean all

.PHONY: all clean fclean re
