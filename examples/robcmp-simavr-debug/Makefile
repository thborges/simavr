# 
# 	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.

target = robcmp-simavr-debug
firm_src = ${wildcard *.c} ${wildcard ../parts/ssd1306*.c} ${wildcard ../parts/hd44780*.c}
simavr = ../../

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim
IPATH += ${simavr}/simavr/sim
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

LDFLAGS += -lpthread

include ../Makefile.opengl

all: obj ${target}

obj:
	mkdir -p ${OBJ}

include ${simavr}/Makefile.common
LDFLAGS := $(filter-out -lsimavr, $(LDFLAGS))

board = ${OBJ}/${target}

${board} : ${OBJ}/${target}.o
${board} : ${OBJ}/ssd1306_virt.o
${board} : ${OBJ}/ssd1306_glut.o
${board} : ${OBJ}/hd44780.o
${board} : ${OBJ}/hd44780_glut.o
	$(CC) $(CFLAGS) $^ ${LIBDIR}/libsimavr.a -o $@ $(LDFLAGS)

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf ${target} *.vcd .*.swo .*.swp .*.swm .*.swn
