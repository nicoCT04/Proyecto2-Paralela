# Proyecto 2 · Computación Paralela y Distribuida (UVG)
#
#   make         compila todos los programas (los de MPI con mpicc)
#   make demo    Parte B.1: cifra y descifra data/texto.txt con la llave 42
#   make clean   borra bin/ y obj/

CC      ?= gcc
MPICC   ?= mpicc
UNAME_S := $(shell uname -s)

# OPENSSL_SUPPRESS_DEPRECATED: la API DES de OpenSSL 3 está marcada como
# obsoleta (DES ya no es seguro), pero sigue disponible y es justo la que
# necesitamos para el proyecto.
CPPFLAGS := -Iinclude -D_POSIX_C_SOURCE=200809L -DOPENSSL_SUPPRESS_DEPRECATED
LDLIBS   := -lcrypto

ifeq ($(UNAME_S),Darwin)
# macOS: OpenSSL de Homebrew no está en las rutas por defecto, y
# _DARWIN_C_SOURCE vuelve a exponer clock_gettime junto a _POSIX_C_SOURCE.
OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null)
CPPFLAGS += -D_DARWIN_C_SOURCE -I$(OPENSSL_PREFIX)/include
LDFLAGS  += -L$(OPENSSL_PREFIX)/lib
CFLAGS   ?= -O2
else
CFLAGS   ?= -O2 -march=native
endif
CFLAGS   += -std=c11 -Wall -Wextra -Wpedantic

COMMON_SRC := src/des_utils.c src/options.c src/problem.c
COMMON_OBJ := $(COMMON_SRC:src/%.c=obj/%.o)
SEQ_BIN    := bin/cifrar bin/secuencial
MPI_OBJ    := obj/search_mpi.o
MPI_BIN    := bin/naive bin/intercalado bin/ciclico bin/maestro_trabajador

.PHONY: all demo clean
.SECONDARY:

all: $(SEQ_BIN) $(MPI_BIN)

$(SEQ_BIN): bin/%: obj/%.o $(COMMON_OBJ) | bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(MPI_BIN): bin/%: src/%.c $(COMMON_OBJ) $(MPI_OBJ) $(wildcard include/*.h) | bin
	$(MPICC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

# El módulo común de MPI incluye mpi.h: se compila con mpicc y solo se
# enlaza en los programas MPI.
$(MPI_OBJ): obj/%.o: src/%.c $(wildcard include/*.h) | obj
	$(MPICC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.c $(wildcard include/*.h) | obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

bin obj:
	mkdir -p $@

demo: bin/cifrar
	./bin/cifrar -f data/texto.txt -k 42 -o resultados/cifrado_42.bin

clean:
	rm -rf bin obj
