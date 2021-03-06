// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2010 - 2013 by the deal.II authors
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


// verify that we can do things like cell->face() in 1d as well. here:
// getting boundary indicators


#include "../tests.h"
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/grid_generator.h>

#include <fstream>


template <int spacedim>
void test ()
{
  Triangulation<1,spacedim> tria;
  GridGenerator::hyper_cube (tria);

  tria.begin_active()->face(0)->set_boundary_indicator(2);
  tria.begin_active()->face(1)->set_boundary_indicator(4);

  deallog << (int)tria.begin_active()->face(0)->boundary_indicator() << std::endl;
  deallog << (int)tria.begin_active()->face(1)->boundary_indicator() << std::endl;
}



int main ()
{
  std::ofstream logfile("vertex_as_face_11/output");
  deallog.attach(logfile);
  deallog.depth_console(0);

  test<1> ();
  test<2> ();

  return 0;
}
