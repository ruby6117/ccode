
controllib = ../../ControlLib
objs = rtl_coprocess/OlfCoprocess.o $(controllib)/controllib.a ProbeComedi.o ../Common/Protocol.o ../Common/Settings.o Server.o ../Common/Log.o ConnThread.o ../Common/Olfactometer.o ../Common/Component.o Conf.o ComediOlfactometer.o ../Common/Common.o Monitor.o ../Common/Lockable.o ConsoleUI.o System.o Curses.o RTLCoprocess.o PIDFlowController.o PolynomialFit.o lm_eval.o lmmin.o ConfParse.o ComediChan.o DAQTaskProxy.o PWMValveProxy.o DataLogableProxy.o Calib.o

.c.o:
	$(CC) -DLINUX -W -Wall -g -I ../Include -c $<

.cpp.o:
	$(CXX) -DLINUX -W -Wall -g -I ../Include -I $(controllib)/include -c $<

OlfactometerServer: $(objs)
	g++ -o OlfactometerServer ProbeComedi.o Protocol.o Server.o Log.o ConnThread.o -lcomedi Settings.o Olfactometer.o Common.o Component.o Conf.o ComediOlfactometer.o Monitor.o Lockable.o ConsoleUI.o System.o Curses.o RTLCoprocess.o PIDFlowController.o PolynomialFit.o lm_eval.o lmmin.o ConfParse.o ComediChan.o DAQTaskProxy.o PWMValveProxy.o DataLogableProxy.o Calib.o -lrt -lpthread -lncurses /usr/lib/libboost_regex.a -lcomedi $(controllib)/controllib.a

rtl_coprocess/OlfCoprocess.o:
	make -C rtl_coprocess

$(controllib)/controllib.a:
	make -C $(controllib) -f Makefile.userspace

clean:
	rm -f *.o *~ OlfactometerServer
	make -C rtl_coprocess clean
	make -C $(controllib) -f Makefile.userspace clean
