# collect all header files, and for each try to do the following:
# generate a file tmp.cc that contains nothing but "#include <xxx>"
# with this particular header file. then try to compile it. this
# sometimes fails since this header does not include all other headers
# it actually needs. we don't usually see this since we may be
# including these missing headers into our files before, so that nobody
# notices the problem. expose it this way.

include ../Makefile.paths
include $D/common/Make.global_options

# The mail program. Override this by something like
#
# MAIL=mymailprog
#
# as part of the 'make' arguments or in the environment to use a different
# mail agent or to tunnel through ssh.  The mail program takes a single
# argument, which is the mail address for tests The contents of the mail will
# be provided on STDIN
MAIL ?= mail


############################################################
# First how to create executables, including all necessary
# flags:
############################################################

flags     = $(CXXFLAGS.g)

ifeq ($(findstring gcc,$(GXX_VERSION)),gcc)
flags += -Wno-missing-noreturn
endif

%.g.$(OBJEXT) : %.cc
	@echo =====debug========= $@
	@$(CXX) $(flags) -c $< -o $@

%.$(OBJEXT) : %.cc
	@echo =====optimized===== $@
	@$(CXX) $(CXXFLAGS.o) -c $< -o $@

%.cc :
	@echo "=================== $@"
	@echo "#include <$^>" > $@
	@echo "// make sure we include config.h somehow:" >> $@
	@echo "DEAL_II_NAMESPACE_OPEN DEAL_II_NAMESPACE_CLOSE" >> $@

%.OK : %.g.$(OBJEXT)
	@echo "=================== $@"
	@touch $@


default: all

# collect all headers
HEADERS = $(shell cd $D; echo include/deal.II/*/*.h)
TESTS   = $(shell for i in $(HEADERS) ; do echo test-$$i | $(PERL) -pi -e 's/.h$$/.OK/g; s/\//-/g;' ; done)

ID = `id -un`"@"`hostname`


# next the target that creates the target rules. First, we list all the header
# files as the head of the rule, followed by a colon. Then the actual rule
# that includes writing the .cc file that only includes a single header file,
# compiles it, and then deletes everything again
Makefile.dep: $(HEADERS:%=$D/%)
	@for i in $(HEADERS) ; do \
		echo `echo test-$$i | $(PERL) -pi -e 's/\.h/.cc/g; s/\//-/g;'` : $D/$$i ; \
	 done > Makefile.dep

-include Makefile.dep


# here's what's to be done
all: $(TESTS)


# create a report, where we get exactly one line per test.
report:
	@-$(MAKE) -k
	@-svn info . | grep '^Revision'
	@echo 'Date: ' `date -u +"%Y %j  %F  %U-%w"`
	@echo 'Id: ' $(ID)
	@for test in $(TESTS) ; do \
		testname=`echo $$test | perl -pi -e 's/.*include.//g; s/\//-/g;'` ; \
		if test -f $$test ; then \
			echo "`date -u +"%Y-%m-%d %H:%M"`   +  $(WORKDIR)/$$testname" ; \
		else \
			echo "`date -u +"%Y-%m-%d %H:%M"`  0   $(WORKDIR)/$$testname" ; \
		fi ; \
	done

report+mail: all
	@$(MAKE) report | tee testresults
	@cat testresults | $(MAIL) dealii.regression.tests@gmail.com
	@rm testresults


clean:
	-rm -f Makefile.dep *.$(OBJEXT) \
               *.g.$(OBJEXT) *.output *~ *.OK *.cc

summary:
	@echo "not implemented"

.PHONY: report clean distclean all default report report+mail summary
