// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2003 - 2013 by the deal.II authors
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



// Just output the embedding matrices of the FE_Q element. Test
// introduced when we started to compute them on the fly, rather than
// precomputing them for a number of elements and storing them in a
// table

#include "../tests.h"
#include <deal.II/base/logstream.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/fe/fe_q.h>

#include <fstream>
#include <string>

#define PRECISION 2



template<int dim>
void
test(const FE_Q<dim> &fe_q)
{
  deallog << fe_q.get_name()
          << std::endl;

  for (unsigned int c=0; c<GeometryInfo<dim>::max_children_per_cell; ++c)
    {
      const FullMatrix<double> &m = fe_q.get_prolongation_matrix(c);

      for (unsigned int i=0; i<m.m(); ++i)
        {
          for (unsigned int j=0; j<m.n(); ++j)
            deallog << 100*m(i,j) << ' ';
          deallog << std::endl;
        }

      deallog << std::endl;
    }

  deallog << std::endl;
}



int
main()
{
  std::ofstream logfile ("q_2/output");
  deallog << std::setprecision(PRECISION);
  deallog << std::fixed;
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  // Test the non-equidistant version as
  // well
  const QGaussLobatto<1> quad(5);

  // we had the matrices precomputed
  // up to Q4 for 1d, Q3 for 2d and
  // Q2 for 3d
  for (unsigned int degree=1; degree<=4; ++degree)
    test<1>(FE_Q<1>(degree));

  test<1>(FE_Q<1>(quad));

  for (unsigned int degree=1; degree<=3; ++degree)
    test<2>(FE_Q<2>(degree));

  test<2>(FE_Q<2>(quad));

  for (unsigned int degree=1; degree<=2; ++degree)
    test<3>(FE_Q<3>(degree));

  test<3>(FE_Q<3>(quad));

  return 0;
}



