// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2009 - 2013 by the deal.II authors
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


// verify that TaskGroup does what we want

#include "../tests.h"
#include <iomanip>
#include <fstream>
#include <unistd.h>

#include <deal.II/base/thread_management.h>


void test (int i)
{
  deallog << "Task " << i << " starting..." << std::endl;
  sleep (1);
  deallog << "Task " << i << " finished!" << std::endl;
}




int main()
{
  std::ofstream logfile("task_07/output");
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  Threads::TaskGroup<> tg;
  tg += Threads::new_task (test, 1);
  tg += Threads::new_task (test, 2);

  tg.join_all ();

  deallog << "OK" << std::endl;
}
