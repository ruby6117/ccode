include ./objs.mk
DBG= -g
#Comment/uncomment for debug
#DBG=

include /usr/rtlinux/rtl.mk

all: ../controllib.o

%controllib.o: $(OBJS)
	ld -r -o ../controllib.o ../precomp/_divdi3.oS ../precomp/_moddi3.oS $(OBJS)
	@echo ""
	@echo "controllib.o built, you can now link it against your realtime module using:"
	@echo "# ld -r -o mymod.o mymod_obj.o controllib.o"
	@echo ""

clean:
	rm -f *.o *~

.cpp.o:
	$(CXX) $(DBG) $(CXXFLAGS) -I../include -nostartfiles -Wall -W -c $<

.c.o:
	$(CC) $(DBG) -DRTLINUX $(CFLAGS) -I../include -c $<

