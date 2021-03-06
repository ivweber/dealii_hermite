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

#ifndef __deal2__fe_face_h
#define __deal2__fe_face_h

#include <deal.II/base/config.h>
#include <deal.II/base/tensor_product_polynomials.h>
#include <deal.II/fe/fe_poly_face.h>

DEAL_II_NAMESPACE_OPEN


/**
 * A finite element, which is a tensor product polynomial on each face
 * and undefined in the interior of the cells. The basis functions on
 * the faces are from Polynomials::LagrangeEquidistant
 *
 * This finite element is the trace space of FE_RaviartThomas on the
 * faces and serves in hybridized methods.
 *
 * @note Since these are only finite elements on faces, only
 * FEFaceValues and FESubfaceValues will be able to extract reasonable
 * values from any face polynomial. In order to make the use of
 * FESystem simpler, FEValues objects will not fail using this finite
 * element space, but all shape function values extracted will equal
 * to zero.
 *
 * @todo Polynomials::LagrangeEquidistant should be and will be
 * replaced by Polynomials::LagrangeGaussLobatto as soon as such a
 * polynomial set exists.
 *
 * @ingroup fe
 * @author Guido Kanschat
 * @date 2009, 2011
 */
template <int dim, int spacedim=dim>
class FE_FaceQ : public FE_PolyFace<TensorProductPolynomials<dim-1>, dim, spacedim>
{
public:
  /**
   * Constructor for tensor product polynomials of degree <tt>p</tt>. The
   * shape functions created using this constructor correspond to Lagrange
   * polynomials in each coordinate direction.
   */
  FE_FaceQ(unsigned int p);

  virtual FiniteElement<dim,spacedim> *clone() const;

  /**
   * Return a string that uniquely identifies a finite element. This class
   * returns <tt>FE_FaceQ<dim>(degree)</tt>, with <tt>dim</tt> and
   * <tt>degree</tt> replaced by appropriate values.
   */
  virtual std::string get_name () const;

  /**
   * Check for non-zero values on a face.
   *
   * This function returns @p true, if the shape function @p shape_index has
   * non-zero values on the face @p face_index.
   *
   * Implementation of the interface in FiniteElement
   */
  virtual bool has_support_on_face (const unsigned int shape_index,
                                    const unsigned int face_index) const;

private:
  /**
   * Return vector with dofs per vertex, line, quad, hex.
   */
  static std::vector<unsigned int> get_dpo_vector (const unsigned int deg);
};

DEAL_II_NAMESPACE_CLOSE

#endif
