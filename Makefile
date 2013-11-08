###############################################################################
# globals
###############################################################################
CFLAGS =	-Wall -Wextra -fPIC -O2 -D_GNU_SOURCE -I./include 	\
				-fvisibility=hidden -Wno-psabi
LDFLAGS =	-Wl,-E
DEBUGFLAGS =
LDSOFLAGS =	-shared -rdynamic
CROSS_COMPILE =
CROSSCC =	$(CROSS_COMPILE)$(CC)

###############################################################################
# libbbus.so
###############################################################################
LIBBBUS_OBJS =		./lib/error.o					\
			./lib/memory.o					\
			./lib/protocol.o				\
			./lib/server.o					\
			./lib/socket.o					\
			./lib/object.o					\
			./lib/client.o					\
			./lib/string.o					\
			./lib/crc32.o					\
			./lib/hashmap.o					\
			./lib/list.o
LIBBBUS_TARGET =	./libbbus.so
LIBBBUS_SONAME =	libbbus.so

libbbus.so:		$(LIBBBUS_OBJS)
	$(CROSSCC) -o $(LIBBBUS_TARGET) $(LIBBBUS_OBJS) $(LDFLAGS)	\
		$(DEBUGFLAGS) $(LDSOFLAGS) -Wl,-soname,$(LIBBBUS_SONAME)

###############################################################################
# bbusd
###############################################################################
BBUSD_OBJS =		./bin/bbusd.o
BBUSD_TARGET =		./bbusd
BBUSD_LIBS =		-lbbus

bbusd:			$(BBUSD_OBJS)
	$(CROSSCC) -o $(BBUSD_TARGET) $(BBUSD_OBJS) $(LDFLAGS)		\
		$(DEBUGFLAGS) $(BBUSD_LIBS) -L./

###############################################################################
# bbus-call
###############################################################################
BBUSCALL_OBJS =		./bin/bbus-call.o
BBUSCALL_TARGET =	./bbus-call
BBUSCALL_LIBS =		-lbbus

bbus-call:		$(BBUSCALL_OBJS)
	$(CROSSCC) -o $(BBUSCALL_TARGET) $(BBUSCALL_OBJS) $(LDFLAGS)	\
		$(DEBUGFLAGS) $(BBUSCALL_LIBS) -L./

###############################################################################
# bbus-echod
###############################################################################
BBUSECHOD_OBJS =	./bin/bbus-echod.o
BBUSECHOD_TARGET =	./bbus-echod
BBUSECHOD_LIBS =	-lbbus

bbus-echod:		$(BBUSECHOD_OBJS)
	$(CROSSCC) -o $(BBUSECHOD_TARGET) $(BBUSECHOD_OBJS) $(LDFLAGS)	\
		$(DEBUGFLAGS) $(BBUSECHOD_LIBS) -L./

###############################################################################
# test
###############################################################################
UNIT_OBJS =	./test/unit/bbus-unit.o					\
		./test/unit/unit_misc.o					\
		./test/unit/unit_hashmap.o				\
		./test/unit/unit_object.o				\
		./test/unit/unit_list.o
UNIT_TARGET =	./bbus-unit
REGR_SCRIPT =	./test/regression/regression.py

bbus-unit:	$(UNIT_OBJS) $(LIBBBUS_OBJS)
	$(CROSSCC) -o $(UNIT_TARGET) $(UNIT_OBJS) $(LIBBBUS_OBJS)	\
		$(LDFLAGS) $(DEBUGFLAGS)

test_unit:	bbus-unit
	$(UNIT_TARGET)

test_regr:	all
	LD_LIBRARY_PATH=./ $(REGR_SCRIPT) run

test:		test_unit test_regr

###############################################################################
# all
###############################################################################
all:		libbbus.so bbusd bbus-call bbus-echod bbus-unit

###############################################################################
# doc
###############################################################################
DOC_DIR =	./doc
doc:
	doxygen

###############################################################################
# clean
###############################################################################
clean:
	rm -f $(BBUSD_OBJS)
	rm -f $(BBUSD_TARGET)
	rm -f $(BBUSCALL_OBJS)
	rm -f $(BBUSCALL_TARGET)
	rm -f $(BBUSECHOD_OBJS)
	rm -f $(BBUSECHOD_TARGET)
	rm -f $(LIBBBUS_OBJS)
	rm -f $(LIBBBUS_TARGET)
	rm -f $(UNIT_OBJS)
	rm -f $(UNIT_TARGET)
	rm -rf $(DOC_DIR)

###############################################################################
# help
###############################################################################
help:
	@echo "Cleaning:"
	@echo "  clean		- delete temporary files created by build"
	@echo
	@echo "Build:"
	@echo "  all		- all executables and libraries"
	@echo "  bbusd		- busybus daemon"
	@echo "  bbus-call	- program for calling busybus methods"
	@echo "  bbus-echod	- busybus echo service daemon"
	@echo "  libbbus.so	- busybus library"
	@echo "  bbus-unit	- busybus unit-test binary"
	@echo
	@echo "Testing:"
	@echo "  test_unit	- build the unit-test suite and run it"
	@echo "  test_regr	- run the regression-tests"
	@echo "  test		- run all tests"
	@echo
	@echo "Documentation:"
	@echo "  doc		- create doxygen documentation"
	@echo

###############################################################################
# other
###############################################################################

.PRECIOUS:	%.c
.SUFFIXES:
.SUFFIXES:	.o .c
.PHONY:		all clean help test doc

.c.o:
	$(CROSSCC) -c -o $*.o $(CFLAGS) $(DEBUGFLAGS) $*.c
