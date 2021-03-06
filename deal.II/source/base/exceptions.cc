// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 1998 - 2013 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the deal.II distribution.
//
// ---------------------------------------------------------------------

#include <deal.II/base/exceptions.h>
#include <deal.II/base/logstream.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef HAVE_GLIBC_STACKTRACE
#  include <execinfo.h>
#endif

#ifdef HAVE_LIBSTDCXX_DEMANGLER
#  include <cxxabi.h>
#endif

DEAL_II_NAMESPACE_OPEN


namespace deal_II_exceptions
{

  std::string additional_assert_output;

  void set_additional_assert_output (const char *const p)
  {
    additional_assert_output = p;
  }

  bool show_stacktrace = true;

  void suppress_stacktrace_in_exceptions ()
  {
    show_stacktrace = false;
  }

  bool abort_on_exception = true;

  void disable_abort_on_exception ()
  {
    abort_on_exception = false;
  }

}



ExceptionBase::ExceptionBase ()
  :
  std::runtime_error(""),
  file(""),
  line(0),
  function(""),
  cond(""),
  exc(""),
  stacktrace (0),
  n_stacktrace_frames (0)
{}



ExceptionBase::ExceptionBase (const ExceptionBase &exc)
  :
  std::runtime_error (exc),
  file(exc.file),
  line(exc.line),
  function(exc.function),
  cond(exc.cond),
  exc(exc.exc),
  stacktrace (0), // don't copy stacktrace to avoid double de-allocation problem
  n_stacktrace_frames (0)
{}



ExceptionBase::~ExceptionBase () throw ()
{
  if (stacktrace != 0)
    {
      free (stacktrace);
      stacktrace = 0;
    }
}



void ExceptionBase::set_fields (const char *f,
                                const int  l,
                                const char *func,
                                const char *c,
                                const char *e)
{
  file = f;
  line = l;
  function = func;
  cond = c;
  exc  = e;

  // if the system supports this, get a stacktrace how we got here
#ifdef HAVE_GLIBC_STACKTRACE
  void *array[25];
  n_stacktrace_frames = backtrace(array, 25);
  stacktrace = backtrace_symbols(array, n_stacktrace_frames);
#endif

  // build up a string with the error message...

  std::ostringstream converter;

  converter << std::endl
            << "--------------------------------------------------------"
            << std::endl;
  // print out general data
  print_exc_data (converter);
  // print out exception specific data
  print_info (converter);
  print_stack_trace (converter);

  if (!deal_II_exceptions::additional_assert_output.empty())
    {
      converter << "--------------------------------------------------------"
                << std::endl
                << deal_II_exceptions::additional_assert_output
                << std::endl;
    }

  converter << "--------------------------------------------------------"
            << std::endl;

  // ... and setup the final error message with it:
  static_cast<std::runtime_error &>(*this) = std::runtime_error(converter.str());
}



const char *ExceptionBase::get_exc_name () const
{
  return exc;
}



void ExceptionBase::print_exc_data (std::ostream &out) const
{
  out << "An error occurred in line <" << line
      << "> of file <" << file
      << "> in function" << std::endl
      << "    " << function << std::endl
      << "The violated condition was: "<< std::endl
      << "    " << cond << std::endl
      << "The name and call sequence of the exception was:" << std::endl
      << "    " << exc  << std::endl
      << "Additional Information: " << std::endl;
}



void ExceptionBase::print_info (std::ostream &out) const
{
  out << "(none)" << std::endl;
}



void ExceptionBase::print_stack_trace (std::ostream &out) const
{
  if (n_stacktrace_frames == 0)
    return;

  if (deal_II_exceptions::show_stacktrace == false)
    return;

  // if there is a stackframe stored, print it
  out << std::endl;
  out << "Stacktrace:" << std::endl
      << "-----------" << std::endl;

  // print the stacktrace. first omit all those frames that have
  // ExceptionBase or deal_II_exceptions in their names, as these
  // correspond to the exception raising mechanism themselves, rather than
  // the place where the exception was triggered
  int frame = 0;
  while ((frame < n_stacktrace_frames)
         &&
         ((std::string(stacktrace[frame]).find ("ExceptionBase") != std::string::npos)
          ||
          (std::string(stacktrace[frame]).find ("deal_II_exceptions") != std::string::npos)))
    ++frame;

  // output the rest
  const unsigned int first_significant_frame = frame;
  for (; frame < n_stacktrace_frames; ++frame)
    {
      out << '#' << frame - first_significant_frame
          << "  ";

      // the stacktrace frame is actually of the format
      // "filename(functionname+offset) [address]". let's try to get the
      // mangled functionname out:
      std::string stacktrace_entry (stacktrace[frame]);
      const unsigned int pos_start = stacktrace_entry.find('('),
                         pos_end   = stacktrace_entry.find('+');
      std::string functionname = stacktrace_entry.substr (pos_start+1,
                                                          pos_end-pos_start-1);

      // demangle, and if successful replace old mangled string by
      // unmangled one (skipping address and offset). treat "main"
      // differently, since it is apparently demangled as "unsigned int"
      // for unknown reasons :-) if we can, demangle the function name
#ifdef HAVE_LIBSTDCXX_DEMANGLER
      int         status;
      char *p = abi::__cxa_demangle(functionname.c_str(), 0, 0, &status);

      if ((status == 0) && (functionname != "main"))
        {
          std::string realname(p);
          // in MT mode, one often gets backtraces spanning several lines
          // because we have so many boost::tuple arguments in the MT
          // calling functions. most of the trailing arguments of these
          // tuples are actually unused boost::tuples::null_type, so we
          // should split them off if they are trailing a template argument
          // list
          while (realname.find (", boost::tuples::null_type>")
                 != std::string::npos)
            realname.erase (realname.find (", boost::tuples::null_type>"),
                            std::string (", boost::tuples::null_type").size());

          stacktrace_entry = stacktrace_entry.substr(0, pos_start)
                             +
                             ": "
                             +
                             realname;
        }
      else
        stacktrace_entry = stacktrace_entry.substr(0, pos_start)
                           +
                           ": "
                           +
                           functionname;

      free (p);

#else

      stacktrace_entry = stacktrace_entry.substr(0, pos_start)
                         +
                         ": "
                         +
                         functionname;
#endif

      // then output what we have
      out << stacktrace_entry
          << std::endl;

      // stop if we're in main()
      if (functionname == "main")
        break;
    }
}



namespace deal_II_exceptions
{
  namespace internals
  {

    void abort (const ExceptionBase &exc)
    {
      if (dealii::deal_II_exceptions::abort_on_exception)
        {
          //* Print the error message and bail out:
          std::cerr << exc.what() << std::endl;
          std::abort();
        }
      else
        {
          throw exc;
        }
    }

  } /*namespace internals*/
} /*namespace deal_II_exceptions*/



DEAL_II_NAMESPACE_CLOSE



// Newer versions of gcc have a very nice feature: you can set a verbose
// terminate handler, that not only aborts a program when an exception is
// thrown and not caught somewhere, but before aborting it prints that an
// exception has been thrown, and possibly what the std::exception::what()
// function has to say. Since many people run into the trap of not having a
// catch clause in main(), they wonder where that abort may be coming from.
// The terminate handler then at least says what is missing in their
// program.
#ifdef HAVE_VERBOSE_TERMINATE
namespace __gnu_cxx
{
  extern void __verbose_terminate_handler ();
}

namespace
{
  struct preload_terminate_dummy
  {
    preload_terminate_dummy()
    {
      std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
    }
  };

  static preload_terminate_dummy dummy;
}
#endif
