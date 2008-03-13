

all: 
	$(MAKE) -C src


clean:
	$(MAKE) -C src clean
	-rm -f *.o *.a *~ include/*~ src/*~
	-rm -fr docs/html

doc:
	doxygen Doxyfile
