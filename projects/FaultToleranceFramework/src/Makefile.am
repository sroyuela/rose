include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

INCLUDES = $(ROSE_INCLUDES) -I$(top_srcdir)/projects/FaultToleranceFramework/include

noinst_LIBRARIES = libroseft.a

libroseft_a_SOURCES = \
			TI.C \
			Transform/ft_trans.C \
			FaultHandlingPolicy.C \
			FaultHandlingSpec.C

LDADD = \
	$(ROSE_LIBS) 

SUBDIRS = Algorithms Analysis Transform


clean-local:
	rm -f *.o *.a
