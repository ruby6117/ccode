include /usr/rtlinux/rtl.mk
controllibpath:=../../../ControlLib
objs = $(controllibpath)/controllib.o module.o Module.o K_PIDFlowController.o K_PWMValve.o K_DAQTask.o K_DataLogable.o K_DataLogger.o


OlfCoprocess.o: $(objs)
	$(LD) -r -o OlfCoprocess.o $(objs) 

$(controllibpath)/controllib.o:
	make -C $(controllibpath)

clean:
	-rm -f *.o *~
	make -C $(controllibpath) clean 

.c.o:
	$(CC) -c $(CFLAGS) -I $(controllibpath)/include $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -I $(controllibpath)/include $<


	
