MAJOR := 0
MINOR := 1
NAME := fs_dotnet_io_api
VERSION := $(MAJOR).$(MINOR)
TARGET_LIB := lib$(NAME).so.$(VERSION)


CC = @arm-linux-gcc

CFLAGS = -fPIC -Wall -g
LDFLAGS = -shared -Wl,-soname,lib$(NAME).so.$(MAJOR)
RM = rm -f

SRCS = fs_dotnet_io_api.c net_i2c_api.c net_spi_api.c
OBJS = $(SRCS:.c=.o)

lib: ${TARGET_LIB}

.PHONY: all
all: ${TARGET_LIB}

${TARGET_LIB}: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@
include $(SRCS:.c=.d)

.PHONY: clean
clean:
	@${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)

