// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2007 - 2013 by the deal.II authors
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



// check FullMatrix::equ (6)


#include "../tests.h"
#include "full_matrix_common.h"


std::string output_file_name = "full_matrix_39/output";


template <typename number>
void
check ()
{
  FullMatrix<number> m,n,o,p;
  make_matrix (m);
  make_matrix (n);
  make_matrix (o);
  make_matrix (p);
  m.equ (3.1415, n, 2.718, o, 1.414, p);
  print_matrix (m);
}

