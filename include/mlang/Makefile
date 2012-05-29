##===- projects/mlang/include/mlang/Makefile ---------------*- Makefile -*-===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = ../..
DIRS = Diag 

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

install-local::
	$(Echo) Installing Mlang include files
	$(Verb) $(MKDIR) $(DESTDIR)$(PROJ_includedir)
	$(Verb) if test -d "$(PROJ_SRC_ROOT)/tools/mlang/include/mlang" ; then \
	  cd $(PROJ_SRC_ROOT)/tools/mlang/include && \
	  for  hdr in `find mlang -type f '!' '(' -name '*~' \
	      -o -name '.#*' -o -name '*.in' -o -name '*.txt' \
	      -o -name 'Makefile' -o -name '*.td' -o -name '*.orig' ')' -print \
              | grep -v CVS | grep -v .svn | grep -v .dir` ; do \
	    instdir=$(DESTDIR)`dirname "$(PROJ_includedir)/$$hdr"` ; \
	    if test \! -d "$$instdir" ; then \
	      $(EchoCmd) Making install directory $$instdir ; \
	      $(MKDIR) $$instdir ;\
	    fi ; \
	    $(DataInstall) $$hdr $(DESTDIR)$(PROJ_includedir)/$$hdr ; \
	  done ; \
	fi
ifneq ($(PROJ_SRC_ROOT),$(PROJ_OBJ_ROOT))
	$(Verb) if test -d "$(PROJ_OBJ_ROOT)/tools/mlang/include/mlang" ; then \
	  cd $(PROJ_OBJ_ROOT)/tools/mlang/include && \
	  for hdr in `find mlang -type f '!' '(' -name 'Makefile' ')' -print \
            | grep -v CVS | grep -v .tmp | grep -v .dir` ; do \
	    $(DataInstall) $$hdr $(DESTDIR)$(PROJ_includedir)/$$hdr ; \
	  done ; \
	fi
endif
