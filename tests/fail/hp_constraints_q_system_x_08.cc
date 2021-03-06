// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2006 - 2013 by the deal.II authors
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



// check that computation of hp constraints works for FESystem(FE_Q) elements correctly
// on a uniformly refined mesh for functions of degree q

// these tests check that we can deal with FESystem(FE_Q(p),FE_DGQ(q)) for
// different p,q

char logname[] = "hp_constraints_q_system_x_08/output";


#include "../hp/hp_constraints_common.h"


template <int dim>
void test ()
{
  hp::FECollection<dim> fe;
  for (unsigned int i=1; i<4; ++i)
    for (unsigned int j=0; j<4; ++j)
      fe.push_back (FESystem<dim>(FE_Q<dim>(i), 1,
                                  FE_DGQ<dim>(j), 1));

  test_with_hanging_nodes_random_aniso (fe);
}
