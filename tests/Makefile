# Generated automatically from Makefile.in by configure.
# Makefile,v 1.14 2002/09/19 13:53:47 wolf Exp
# Common Makefile for all test directories

include Makefile.paths
include $D/common/Make.global_options

# first define the directories we should work on
nofail-dirs = a-framework       \
	      base              \
              lac               \
              fe                \
              deal.II           \
              integrators       \
              multigrid         \
              grid              \
              bits              \
              hp                \
              matrix_free       \
              aniso             \
              codim_one         \
              serialization     \
	      mesh_converter    \
	      $(if $(DEAL_II_USE_MPI:no=),      $(if $(USE_CONTRIB_P4EST:no=),mpi,),) \
	      $(if $(DEAL_II_USE_MPI:no=),      $(if $(USE_CONTRIB_P4EST:no=),gla,),) \
	      $(if $(USE_CONTRIB_P4EST:no=),    distributed_grids,) \
	      $(if $(USE_CONTRIB_PETSC:no=),    petsc,) \
	      $(if $(USE_CONTRIB_SLEPC:no=),    slepc,) \
	      $(if $(USE_CONTRIB_TRILINOS:no=), trilinos,) \
	      $(if $(USE_CONTRIB_UMFPACK:no=),  umfpack,) \
	      $(if $(USE_CONTRIB_METIS:no=),    metis,) \
	      $(if $(USE_CONTRIB_HSL:no=),      hsl,) \
	      $(if $(USE_CONTRIB_LAPACK:no=),   lapack,)

# now define the targets that actually do something
all: nofail do-fail

nofail: $(addprefix do-,$(nofail-dirs))

do-%:
	cd $(@:do-%=%) && $(MAKE)


report:
	-@$(MAKE) -k nofail
	@for i in $(nofail-dirs) fail all-headers ; do \
		echo =======Report: $$i ======= ; \
		cd $$i ; $(MAKE) report ; cd .. ; \
	 done

report+summary:
	-@$(MAKE) -k nofail
	@for i in $(nofail-dirs) fail all-headers ; do \
		echo =======Report: $$i ======= ; \
		cd $$i ; $(MAKE) report ; cd .. ; \
	 done
	@echo "                Compiling Linking Running   Check      OK     all"
	@for i in $(nofail-dirs) fail all-headers ; do \
		printf "%20s\t" "$$i"; \
		cd $$i ; $(MAKE) -s summary ; cd .. ; \
	 done

report+mail:
	-@$(MAKE) -k nofail
	@for i in $(nofail-dirs) fail all-headers ; do \
		echo =======Report: $$i ======= ; \
		cd $$i ; $(MAKE) report+mail ; cd .. ; \
	 done

# compilation of tests only, no execution of tests
build: $(addprefix build-,$(nofail-dirs) fail)
build-%:
	cd $(@:build-%=%) && $(MAKE) build


# compilation and execution of tests only, no comparison with
# reference values
output: $(addprefix output-,$(nofail-dirs) fail)
output-%:
	cd $(@:output-%=%) && $(MAKE) output


# update/generate references
refs: $(addprefix output-,$(nofail-dirs))
refs-%:
	cd $(@:refs-%=%) && $(MAKE) refs


# clean up output files
clean:     $(addprefix clean-,$(nofail-dirs) fail)
distclean: $(addprefix distclean-,$(nofail-dirs) fail)
clean-%:
	cd $(@:clean-%=%) && $(MAKE) clean
distclean-%:
	cd $(@:distclean-%=%) && $(MAKE) distclean



resultname:
	@echo $(TARGET)+$(GXX-VERSION)


.PHONY : all do-% report report+mail build build-% refs refs-% \
         clean clean-% \
	 distclean distclean-% \
	 resultname


