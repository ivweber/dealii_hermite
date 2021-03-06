// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2001 - 2013 by the deal.II authors
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


// check GridTools::volume


#include "../tests.h"
#include <deal.II/base/logstream.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_boundary_lib.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/grid_out.h>

#include <fstream>
#include <iomanip>

std::ofstream logfile("grid_tools_03/output");



template <int dim>
void test1 ()
{
  // test 1: hypercube
  if (true)
    {
      Triangulation<dim> tria;
      GridGenerator::hyper_cube(tria);

      for (unsigned int i=0; i<2; ++i)
        {
          tria.refine_global(2);
          deallog << dim << "d, "
                  << "hypercube volume, "
                  << i*2
                  << " refinements: "
                  << GridTools::volume (tria)
                  << std::endl;
        };
    };

  // test 2: hyperball
  if (dim >= 2)
    {
      Triangulation<dim> tria;
      GridGenerator::hyper_ball(tria, Point<dim>(), 1);

      static const HyperBallBoundary<dim> boundary;
      tria.set_boundary (0, boundary);

      for (unsigned int i=0; i<4; ++i)
        {
          tria.refine_global(1);
          deallog << dim << "d, "
                  << "hyperball volume, "
                  << i
                  << " refinements: "
                  << GridTools::volume (tria)
                  << std::endl;
        }
      deallog << "exact value="
              << (dim==2 ? numbers::PI :
                  4./3.*numbers::PI)
              << std::endl;
    }
}


int main ()
{
  deallog << std::setprecision(4);
  logfile << std::setprecision(4);
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  test1<1> ();
  test1<2> ();
  test1<3> ();

  return 0;
}

