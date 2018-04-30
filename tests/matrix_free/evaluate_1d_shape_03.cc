// ---------------------------------------------------------------------
//
// Copyright (C) 2013 - 2018 by the deal.II authors
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



// check the correctness of the 1d evaluation functions used in FEEvaluation,
// path evaluate_symmetric_hierarchical

#include "../tests.h"
#include <iostream>

#include <deal.II/matrix_free/tensor_product_kernels.h>


template <int M, int N, int type, bool add>
void test()
{
  deallog << "Test " << M << " x " << N << std::endl;
  AlignedVector<double> shape(M*N);
  for (unsigned int i=0; i<M; ++i)
    for (unsigned int j=0; j<(N+1)/2; ++j)
      {
        shape[i*N+j] = -1. + 2. * (double)Testing::rand()/RAND_MAX;
        if (((i+type)%2)==1)
          shape[i*N+N-1-j] = -shape[i*N+j];
        else
          shape[i*N+N-1-j] = shape[i*N+j];
        if (j==N/2 && ((i+type)%2)==1)
          shape[i*N+j] = 0.;
      }

  double x[N], x_ref[N], y[M], y_ref[M];
  for (unsigned int i=0; i<N; ++i)
    x[i] = (double)Testing::rand()/RAND_MAX;

  // compute reference
  for (unsigned int i=0; i<M; ++i)
    {
      y[i] = 1.;
      y_ref[i] = add ? y[i] : 0.;
      for (unsigned int j=0; j<N; ++j)
        y_ref[i] += shape[i*N+j] * x[j];
    }

  // apply function for tensor product
  internal::EvaluatorTensorProduct<internal::evaluate_symmetric_hierarchical,1,M,N,double> evaluator(shape, shape, shape);
  if (type == 0)
    evaluator.template values<0,false,add> (x,y);
  if (type == 1)
    evaluator.template gradients<0,false,add> (x,y);
  if (type == 2)
    evaluator.template hessians<0,false,add> (x,y);


  deallog << "Errors no transpose: ";
  for (unsigned int i=0; i<M; ++i)
    deallog << y[i] - y_ref[i] << " ";
  deallog << std::endl;


  for (unsigned int i=0; i<M; ++i)
    y[i] = (double)Testing::rand()/RAND_MAX;

  // compute reference
  for (unsigned int i=0; i<N; ++i)
    {
      x[i] = 2.;
      x_ref[i] = add ? x[i] : 0.;
      for (unsigned int j=0; j<M; ++j)
        x_ref[i] += shape[j*N+i] * y[j];
    }

  // apply function for tensor product
  if (type == 0)
    evaluator.template values<0,true,add> (y,x);
  if (type == 1)
    evaluator.template gradients<0,true,add> (y,x);
  if (type == 2)
    evaluator.template hessians<0,true,add> (y,x);

  deallog << "Errors transpose:    ";
  for (unsigned int i=0; i<N; ++i)
    deallog << x[i] - x_ref[i] << " ";
  deallog << std::endl;
}

int main ()
{
  initlog();

  deallog.push("values");
  test<4,4,0,false>();
  test<3,3,0,false>();
  test<4,3,0,false>();
  test<3,4,0,false>();
  test<3,5,0,false>();
  deallog.pop();

  deallog.push("gradients");
  test<4,4,1,false>();
  test<3,3,1,false>();
  test<4,3,1,false>();
  test<3,4,1,false>();
  test<3,5,1,false>();
  deallog.pop();

  deallog.push("hessians");
  test<4,4,2,false>();
  test<3,3,2,false>();
  test<4,3,2,false>();
  test<3,4,2,false>();
  test<3,5,2,false>();
  deallog.pop();

  deallog.push("add");

  deallog.push("values");
  test<4,4,0,true>();
  test<3,3,0,true>();
  test<4,3,0,true>();
  test<3,4,0,true>();
  test<3,5,0,true>();
  deallog.pop();

  deallog.push("gradients");
  test<4,4,1,true>();
  test<3,3,1,true>();
  test<4,3,1,true>();
  test<3,4,1,true>();
  test<3,5,1,true>();
  deallog.pop();

  deallog.push("hessians");
  test<4,4,2,true>();
  test<3,3,2,true>();
  test<4,3,2,true>();
  test<3,4,2,true>();
  test<3,5,2,true>();
  deallog.pop();

  deallog.pop();

  return 0;
}