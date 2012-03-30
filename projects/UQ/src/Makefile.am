include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

INCLUDES = $(ROSE_INCLUDES) -I$(top_srcdir)/projects/UQ/include

CXX_TEMPLATE_REPOSITORY_PATH = .

bin_PROGRAMS = uq

uq_SOURCES = uq.C TI.C

LDADD = $(ROSE_LIBS) 

clean-local:
	rm -f *.o
