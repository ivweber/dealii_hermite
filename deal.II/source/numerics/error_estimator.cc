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

#include <deal.II/base/thread_management.h>
#include <deal.II/base/quadrature.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/work_stream.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/parallel_vector.h>
#include <deal.II/lac/block_vector.h>
#include <deal.II/lac/parallel_block_vector.h>
#include <deal.II/lac/petsc_vector.h>
#include <deal.II/lac/petsc_block_vector.h>
#include <deal.II/lac/trilinos_vector.h>
#include <deal.II/lac/trilinos_block_vector.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/base/geometry_info.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/hp/fe_values.h>
#include <deal.II/fe/fe_update_flags.h>
#include <deal.II/fe/mapping_q1.h>
#include <deal.II/hp/q_collection.h>
#include <deal.II/hp/mapping_collection.h>
#include <deal.II/numerics/error_estimator.h>

#include <deal.II/base/std_cxx1x/bind.h>

#include <numeric>
#include <algorithm>
#include <cmath>
#include <vector>

DEAL_II_NAMESPACE_OPEN


namespace
{
  inline
  double sqr (const double x)
  {
    return x*x;
  }


  template <typename CellIterator>
  inline
  void advance_by_n (CellIterator &cell,
                     const unsigned int n)
  {
    // store a pointer to the end
    // iterator, since we can't get at
    // it any more once cell is already
    // the end iterator (in that case
    // dereferencing cell-> triggers an
    // assertion)
    const CellIterator endc = cell->get_dof_handler().end();
    for (unsigned int t=0; ((t<n) && (cell!=endc)); ++t, ++cell)
      ;
  }
}


namespace internal
{
  namespace
  {
    /**
     * All small temporary data
     * objects that are needed once
     * per thread by the several
     * functions of the error
     * estimator are gathered in this
     * struct. The reason for this
     * structure is mainly that we
     * have a number of functions
     * that operate on cells or faces
     * and need a number of small
     * temporary data objects. Since
     * these functions may run in
     * parallel, we cannot make these
     * objects member variables of
     * the enclosing class. On the
     * other hand, declaring them
     * locally in each of these
     * functions would require their
     * reallocating every time we
     * visit the next cell or face,
     * which we found can take a
     * significant amount of time if
     * it happens often even in the
     * single threaded case (10-20
     * per cent in our measurements);
     * however, most importantly,
     * memory allocation requires
     * synchronisation in
     * multithreaded mode. While that
     * is done by the C++ library and
     * has not to be handcoded, it
     * nevertheless seriously damages
     * the ability to efficiently run
     * the functions of this class in
     * parallel, since they are quite
     * often blocked by these
     * synchronisation points,
     * slowing everything down by a
     * factor of two or three.
     *
     * Thus, every thread gets an
     * instance of this class to work
     * with and needs not allocate
     * memory itself, or synchronise
     * with other threads.
     *
     * The sizes of the arrays are
     * initialized with the maximal number of
     * entries necessary for the hp
     * case. Within the loop over individual
     * cells, we then resize the arrays as
     * necessary. Since for std::vector
     * resizing to a smaller size doesn't
     * imply memory allocation, this is fast.
     */
    template <class DH>
    struct ParallelData
    {
      static const unsigned int dim      = DH::dimension;
      static const unsigned int spacedim = DH::space_dimension;

      /**
       * The finite element to be used.
       */
      const dealii::hp::FECollection<dim,spacedim> finite_element;

      /**
       * The quadrature formulas to be used for
       * the faces.
       */
      const dealii::hp::QCollection<dim-1> face_quadratures;

      /**
       * FEFaceValues objects to integrate over
       * the faces of the current and
       * potentially of neighbor cells.
       */
      dealii::hp::FEFaceValues<dim,spacedim>    fe_face_values_cell;
      dealii::hp::FEFaceValues<dim,spacedim>    fe_face_values_neighbor;
      dealii::hp::FESubfaceValues<dim,spacedim> fe_subface_values;

      /**
       * A vector to store the jump
       * of the normal vectors in
       * the quadrature points for
       * each of the solution
       * vectors (i.e. a temporary
       * value). This vector is not
       * allocated inside the
       * functions that use it, but
       * rather globally, since
       * memory allocation is slow,
       * in particular in presence
       * of multiple threads where
       * synchronisation makes
       * things even slower.
       */
      std::vector<std::vector<std::vector<double> > > phi;

      /**
       * A vector for the gradients of
       * the finite element function
       * on one cell
       *
       * Let psi be a short name
       * for <tt>a grad u_h</tt>, where
       * the third index be the
       * component of the finite
       * element, and the second
       * index the number of the
       * quadrature point. The
       * first index denotes the
       * index of the solution
       * vector.
       */
      std::vector<std::vector<std::vector<Tensor<1,spacedim> > > > psi;

      /**
       * The same vector for a neighbor cell
       */
      std::vector<std::vector<std::vector<Tensor<1,spacedim> > > > neighbor_psi;

      /**
       * The normal vectors of the finite
       * element function on one face
       */
      std::vector<Point<spacedim> > normal_vectors;

      /**
       * Two arrays needed for the
       * values of coefficients in
       * the jumps, if they are
       * given.
       */
      std::vector<double>                  coefficient_values1;
      std::vector<dealii::Vector<double> > coefficient_values;

      /**
       * Array for the products of
       * Jacobian determinants and
       * weights of quadraturs
       * points.
       */
      std::vector<double>          JxW_values;

      /**
       * The subdomain id we are to care
       * for.
       */
      const types::subdomain_id subdomain_id;
      /**
       * The material id we are to care
       * for.
       */
      const types::material_id material_id;

      /**
       * Some more references to input data to
       * the KellyErrorEstimator::estimate()
       * function.
       */
      const typename FunctionMap<spacedim>::type *neumann_bc;
      const ComponentMask                component_mask;
      const Function<spacedim>                   *coefficients;

      /**
       * Constructor.
       */
      template <class FE>
      ParallelData (const FE                                           &fe,
                    const dealii::hp::QCollection<DH::dimension-1>               &face_quadratures,
                    const dealii::hp::MappingCollection<DH::dimension, DH::space_dimension> &mapping,
                    const bool         need_quadrature_points,
                    const unsigned int n_solution_vectors,
                    const types::subdomain_id subdomain_id,
                    const types::material_id material_id,
                    const typename FunctionMap<spacedim>::type *neumann_bc,
                    const ComponentMask                component_mask,
                    const Function<spacedim>                   *coefficients);

      /**
       * Resize the arrays so that they fit the
       * number of quadrature points associated
       * with the given finite element index
       * into the hp collections.
       */
      void resize (const unsigned int active_fe_index);
    };


    template <class DH>
    template <class FE>
    ParallelData<DH>::
    ParallelData (const FE                                           &fe,
                  const dealii::hp::QCollection<DH::dimension-1>     &face_quadratures,
                  const dealii::hp::MappingCollection<DH::dimension, DH::space_dimension> &mapping,
                  const bool     need_quadrature_points,
                  const unsigned int n_solution_vectors,
                  const types::subdomain_id subdomain_id,
                  const types::material_id material_id,
                  const typename FunctionMap<spacedim>::type *neumann_bc,
                  const ComponentMask                component_mask,
                  const Function<spacedim>         *coefficients)
      :
      finite_element (fe),
      face_quadratures (face_quadratures),
      fe_face_values_cell (mapping,
                           finite_element,
                           face_quadratures,
                           update_gradients      |
                           update_JxW_values     |
                           (need_quadrature_points  ?
                            update_quadrature_points :
                            UpdateFlags()) |
                           update_normal_vectors),
      fe_face_values_neighbor (mapping,
                               finite_element,
                               face_quadratures,
                               update_gradients),
      fe_subface_values (mapping,
                         finite_element,
                         face_quadratures,
                         update_gradients),
      phi (n_solution_vectors,
           std::vector<std::vector<double> >
           (face_quadratures.max_n_quadrature_points(),
            std::vector<double> (fe.n_components()))),
      psi (n_solution_vectors,
           std::vector<std::vector<Tensor<1,spacedim> > >
           (face_quadratures.max_n_quadrature_points(),
            std::vector<Tensor<1,spacedim> > (fe.n_components()))),
      neighbor_psi (n_solution_vectors,
                    std::vector<std::vector<Tensor<1,spacedim> > >
                    (face_quadratures.max_n_quadrature_points(),
                     std::vector<Tensor<1,spacedim> > (fe.n_components()))),
      normal_vectors (face_quadratures.max_n_quadrature_points()),
      coefficient_values1 (face_quadratures.max_n_quadrature_points()),
      coefficient_values (face_quadratures.max_n_quadrature_points(),
                          dealii::Vector<double> (fe.n_components())),
      JxW_values (face_quadratures.max_n_quadrature_points()),
      subdomain_id (subdomain_id),
      material_id (material_id),
      neumann_bc (neumann_bc),
      component_mask (component_mask),
      coefficients (coefficients)
    {}



    template <class DH>
    void
    ParallelData<DH>::resize (const unsigned int active_fe_index)
    {
      const unsigned int n_q_points   = face_quadratures[active_fe_index].size();
      const unsigned int n_components = finite_element.n_components();

      normal_vectors.resize(n_q_points);
      coefficient_values1.resize(n_q_points);
      coefficient_values.resize(n_q_points);
      JxW_values.resize(n_q_points);

      for (unsigned int i=0; i<phi.size(); ++i)
        {
          phi[i].resize(n_q_points);
          psi[i].resize(n_q_points);
          neighbor_psi[i].resize(n_q_points);

          for (unsigned int qp=0; qp<n_q_points; ++qp)
            {
              phi[i][qp].resize(n_components);
              psi[i][qp].resize(n_components);
              neighbor_psi[i][qp].resize(n_components);
            }
        }

      for (unsigned int qp=0; qp<n_q_points; ++qp)
        coefficient_values[qp].reinit(n_components);
    }



    /**
     * Copy data from the
     * local_face_integrals map of a single
     * ParallelData object into a global such
     * map. This is the copier stage of a
     * WorkStream pipeline.
     */
    template <class DH>
    void
    copy_local_to_global (const std::map<typename DH::face_iterator,std::vector<double> > &local_face_integrals,
                          std::map<typename DH::face_iterator,std::vector<double> > &face_integrals)
    {

      // now copy locally computed elements
      // into the global map
      for (typename std::map<typename DH::face_iterator,std::vector<double> >::const_iterator
           p=local_face_integrals.begin();
           p!=local_face_integrals.end();
           ++p)
        {
          // double check that the
          // element does not already
          // exists in the global map
          Assert (face_integrals.find (p->first) == face_integrals.end(),
                  ExcInternalError());

          for (unsigned int i=0; i<p->second.size(); ++i)
            {
              Assert (numbers::is_finite(p->second[i]), ExcInternalError());
              Assert (p->second[i] >= 0, ExcInternalError());
            }

          face_integrals[p->first] = p->second;
        }
    }


    /**
     * Actually do the computation on
     * a face which has no hanging
     * nodes (it is regular), i.e.
     * either on the other side there
     * is nirvana (face is at
     * boundary), or the other side's
     * refinement level is the same
     * as that of this side, then
     * handle the integration of
     * these both cases together.
     */
    template <typename InputVector, class DH>
    void
    integrate_over_regular_face (const std::vector<const InputVector *>   &solutions,
                                 ParallelData<DH>                        &parallel_data,
                                 std::map<typename DH::face_iterator,std::vector<double> > &local_face_integrals,
                                 const typename DH::active_cell_iterator &cell,
                                 const unsigned int                       face_no,
                                 dealii::hp::FEFaceValues<DH::dimension, DH::space_dimension> &fe_face_values_cell,
                                 dealii::hp::FEFaceValues<DH::dimension, DH::space_dimension> &fe_face_values_neighbor)
    {
      const unsigned int dim = DH::dimension;

      const typename DH::face_iterator face = cell->face(face_no);
      const unsigned int n_q_points         = parallel_data.face_quadratures[cell->active_fe_index()].size(),
                         n_components       = parallel_data.finite_element.n_components(),
                         n_solution_vectors = solutions.size();


      // initialize data of the restriction
      // of this cell to the present face
      fe_face_values_cell.reinit (cell, face_no,
                                  cell->active_fe_index());

      // get gradients of the finite element
      // function on this cell
      for (unsigned int n=0; n<n_solution_vectors; ++n)
        fe_face_values_cell.get_present_fe_values()
        .get_function_gradients (*solutions[n], parallel_data.psi[n]);

      // now compute over the other side of
      // the face
      if (face->at_boundary() == false)
        // internal face; integrate jump
        // of gradient across this face
        {
          Assert (cell->neighbor(face_no).state() == IteratorState::valid,
                  ExcInternalError());

          const typename DH::active_cell_iterator neighbor = cell->neighbor(face_no);

          // find which number the
          // current face has relative to
          // the neighboring cell
          const unsigned int neighbor_neighbor
            = cell->neighbor_of_neighbor (face_no);
          Assert (neighbor_neighbor<GeometryInfo<dim>::faces_per_cell,
                  ExcInternalError());

          // get restriction of finite element
          // function of @p{neighbor} to the
          // common face. in the hp case, use the
          // quadrature formula that matches the
          // one we would use for the present
          // cell
          fe_face_values_neighbor.reinit (neighbor, neighbor_neighbor,
                                          cell->active_fe_index());

          // get gradients on neighbor cell
          for (unsigned int n=0; n<n_solution_vectors; ++n)
            {
              fe_face_values_neighbor.get_present_fe_values()
              .get_function_gradients (*solutions[n],
                                       parallel_data.neighbor_psi[n]);

              // compute the jump in the gradients
              for (unsigned int component=0; component<n_components; ++component)
                for (unsigned int p=0; p<n_q_points; ++p)
                  parallel_data.psi[n][p][component] -= parallel_data.neighbor_psi[n][p][component];
            }
        }


      // now psi contains the following:
      // - for an internal face, psi=[grad u]
      // - for a neumann boundary face,
      //   psi=grad u
      // each component being the
      // mentioned value at one of the
      // quadrature points

      // next we have to multiply this with
      // the normal vector. Since we have
      // taken the difference of gradients
      // for internal faces, we may chose
      // the normal vector of one cell,
      // taking that of the neighbor
      // would only change the sign. We take
      // the outward normal.

      parallel_data.normal_vectors =
        fe_face_values_cell.get_present_fe_values().get_normal_vectors();

      for (unsigned int n=0; n<n_solution_vectors; ++n)
        for (unsigned int component=0; component<n_components; ++component)
          for (unsigned int point=0; point<n_q_points; ++point)
            parallel_data.phi[n][point][component]
              = (parallel_data.psi[n][point][component] *
                 parallel_data.normal_vectors[point]);

      // if a coefficient was given: use that
      // to scale the jump in the gradient
      if (parallel_data.coefficients != 0)
        {
          // scalar coefficient
          if (parallel_data.coefficients->n_components == 1)
            {

              parallel_data.coefficients
              ->value_list (fe_face_values_cell.get_present_fe_values()
                            .get_quadrature_points(),
                            parallel_data.coefficient_values1);
              for (unsigned int n=0; n<n_solution_vectors; ++n)
                for (unsigned int component=0; component<n_components; ++component)
                  for (unsigned int point=0; point<n_q_points; ++point)
                    parallel_data.phi[n][point][component] *=
                      parallel_data.coefficient_values1[point];
            }
          else
            // vector-valued coefficient
            {
              parallel_data.coefficients
              ->vector_value_list (fe_face_values_cell.get_present_fe_values()
                                   .get_quadrature_points(),
                                   parallel_data.coefficient_values);
              for (unsigned int n=0; n<n_solution_vectors; ++n)
                for (unsigned int component=0; component<n_components; ++component)
                  for (unsigned int point=0; point<n_q_points; ++point)
                    parallel_data.phi[n][point][component] *=
                      parallel_data.coefficient_values[point](component);
            }
        }


      if (face->at_boundary() == true)
        // neumann boundary face. compute
        // difference between normal
        // derivative and boundary function
        {
          const types::boundary_id boundary_indicator = face->boundary_indicator();

          Assert (parallel_data.neumann_bc->find(boundary_indicator) !=
                  parallel_data.neumann_bc->end(),
                  ExcInternalError ());
          // get the values of the boundary
          // function at the quadrature
          // points
          if (n_components == 1)
            {
              std::vector<double> g(n_q_points);
              parallel_data.neumann_bc->find(boundary_indicator)->second
              ->value_list (fe_face_values_cell.get_present_fe_values()
                            .get_quadrature_points(), g);

              for (unsigned int n=0; n<n_solution_vectors; ++n)
                for (unsigned int point=0; point<n_q_points; ++point)
                  parallel_data.phi[n][point][0] -= g[point];
            }
          else
            {
              std::vector<dealii::Vector<double> >
              g(n_q_points, dealii::Vector<double>(n_components));
              parallel_data.neumann_bc->find(boundary_indicator)->second
              ->vector_value_list (fe_face_values_cell.get_present_fe_values()
                                   .get_quadrature_points(),
                                   g);

              for (unsigned int n=0; n<n_solution_vectors; ++n)
                for (unsigned int component=0; component<n_components; ++component)
                  for (unsigned int point=0; point<n_q_points; ++point)
                    parallel_data.phi[n][point][component] -= g[point](component);
            }
        }


      // now phi contains the following:
      // - for an internal face, phi=[a du/dn]
      // - for a neumann boundary face,
      //   phi=a du/dn-g
      // each component being the
      // mentioned value at one of the
      // quadrature points

      parallel_data.JxW_values
        = fe_face_values_cell.get_present_fe_values().get_JxW_values();

      // take the square of the phi[i]
      // for integration, and sum up
      std::vector<double> face_integral (n_solution_vectors, 0);
      for (unsigned int n=0; n<n_solution_vectors; ++n)
        for (unsigned int component=0; component<n_components; ++component)
          if (parallel_data.component_mask[component] == true)
            for (unsigned int p=0; p<n_q_points; ++p)
              face_integral[n] += dealii::sqr(parallel_data.phi[n][p][component]) *
                                  parallel_data.JxW_values[p];

      local_face_integrals[face] = face_integral;
    }




    /**
     * The same applies as for the
     * function above, except that
     * integration is over face
     * @p face_no of @p cell, where
     * the respective neighbor is
     * refined, so that the
     * integration is a bit more
     * complex.
     */
    template <typename InputVector, class DH>
    void
    integrate_over_irregular_face (const std::vector<const InputVector *>      &solutions,
                                   ParallelData<DH>                           &parallel_data,
                                   std::map<typename DH::face_iterator,std::vector<double> > &local_face_integrals,
                                   const typename DH::active_cell_iterator    &cell,
                                   const unsigned int                          face_no,
                                   dealii::hp::FEFaceValues<DH::dimension,DH::space_dimension>    &fe_face_values,
                                   dealii::hp::FESubfaceValues<DH::dimension, DH::space_dimension> &fe_subface_values)
    {
      const unsigned int dim = DH::dimension;

      const typename DH::cell_iterator neighbor = cell->neighbor(face_no);
      const unsigned int n_q_points         = parallel_data.face_quadratures[cell->active_fe_index()].size(),
                         n_components       = parallel_data.finite_element.n_components(),
                         n_solution_vectors = solutions.size();
      const typename DH::face_iterator
      face=cell->face(face_no);

      Assert (neighbor.state() == IteratorState::valid, ExcInternalError());
      Assert (face->has_children(), ExcInternalError());
      // set up a vector of the gradients
      // of the finite element function
      // on this cell at the quadrature
      // points
      //
      // let psi be a short name for
      // [a grad u_h], where the second
      // index be the component of the
      // finite element, and the first
      // index the number of the
      // quadrature point

      // store which number @p{cell} has
      // in the list of neighbors of
      // @p{neighbor}
      const unsigned int neighbor_neighbor
        = cell->neighbor_of_neighbor (face_no);
      Assert (neighbor_neighbor<GeometryInfo<dim>::faces_per_cell,
              ExcInternalError());

      // loop over all subfaces
      for (unsigned int subface_no=0; subface_no<face->n_children(); ++subface_no)
        {
          // get an iterator pointing to the
          // cell behind the present subface
          const typename DH::active_cell_iterator neighbor_child
            = cell->neighbor_child_on_subface (face_no, subface_no);
          Assert (!neighbor_child->has_children(),
                  ExcInternalError());

          // restrict the finite element
          // on the present cell to the
          // subface
          fe_subface_values.reinit (cell, face_no, subface_no,
                                    cell->active_fe_index());

          // restrict the finite element
          // on the neighbor cell to the
          // common @p{subface}.
          fe_face_values.reinit (neighbor_child, neighbor_neighbor,
                                 cell->active_fe_index());

          // store the gradient of the
          // solution in psi
          for (unsigned int n=0; n<n_solution_vectors; ++n)
            fe_subface_values.get_present_fe_values()
            .get_function_gradients (*solutions[n], parallel_data.psi[n]);

          // store the gradient from the
          // neighbor's side in
          // @p{neighbor_psi}
          for (unsigned int n=0; n<n_solution_vectors; ++n)
            fe_face_values.get_present_fe_values()
            .get_function_gradients (*solutions[n], parallel_data.neighbor_psi[n]);

          // compute the jump in the gradients
          for (unsigned int n=0; n<n_solution_vectors; ++n)
            for (unsigned int component=0; component<n_components; ++component)
              for (unsigned int p=0; p<n_q_points; ++p)
                parallel_data.psi[n][p][component] -=
                  parallel_data.neighbor_psi[n][p][component];

          // note that unlike for the
          // case of regular faces
          // (treated in the other
          // function of this class), we
          // have not to take care of
          // boundary faces here, since
          // they always are regular.

          // next we have to multiply this with
          // the normal vector. Since we have
          // taken the difference of gradients
          // for internal faces, we may chose
          // the normal vector of one cell,
          // taking that of the neighbor
          // would only change the sign. We take
          // the outward normal.
          //
          // let phi be the name of the integrand

          parallel_data.normal_vectors
            = fe_face_values.get_present_fe_values().get_normal_vectors();


          for (unsigned int n=0; n<n_solution_vectors; ++n)
            for (unsigned int component=0; component<n_components; ++component)
              for (unsigned int point=0; point<n_q_points; ++point)
                parallel_data.phi[n][point][component] = (parallel_data.psi[n][point][component]*
                                                          parallel_data.normal_vectors[point]);

          // if a coefficient was given: use that
          // to scale the jump in the gradient
          if (parallel_data.coefficients != 0)
            {
              // scalar coefficient
              if (parallel_data.coefficients->n_components == 1)
                {
                  parallel_data.coefficients
                  ->value_list (fe_face_values.get_present_fe_values()
                                .get_quadrature_points(),
                                parallel_data.coefficient_values1);
                  for (unsigned int n=0; n<n_solution_vectors; ++n)
                    for (unsigned int component=0; component<n_components; ++component)
                      for (unsigned int point=0; point<n_q_points; ++point)
                        parallel_data.phi[n][point][component] *=
                          parallel_data.coefficient_values1[point];
                }
              else
                // vector-valued coefficient
                {
                  parallel_data.coefficients
                  ->vector_value_list (fe_face_values.get_present_fe_values()
                                       .get_quadrature_points(),
                                       parallel_data.coefficient_values);
                  for (unsigned int n=0; n<n_solution_vectors; ++n)
                    for (unsigned int component=0; component<n_components; ++component)
                      for (unsigned int point=0; point<n_q_points; ++point)
                        parallel_data.phi[n][point][component] *=
                          parallel_data.coefficient_values[point](component);
                }
            }

          // get the weights for the
          // integration. note that it
          // does not matter whether we
          // take the JxW values from the
          // fe_face_values or the
          // fe_subface_values, as the
          // first is on the small
          // neighbor cell, while the
          // latter is on the refined
          // face of the big cell here
          parallel_data.JxW_values
            = fe_face_values.get_present_fe_values().get_JxW_values();

          // take the square of the phi[i]
          // for integration, and sum up
          std::vector<double> face_integral (n_solution_vectors, 0);
          for (unsigned int n=0; n<n_solution_vectors; ++n)
            for (unsigned int component=0; component<n_components; ++component)
              if (parallel_data.component_mask[component] == true)
                for (unsigned int p=0; p<n_q_points; ++p)
                  face_integral[n] += dealii::sqr(parallel_data.phi[n][p][component]) *
                                      parallel_data.JxW_values[p];

          local_face_integrals[neighbor_child->face(neighbor_neighbor)]
            = face_integral;
        }


      // finally loop over all subfaces to
      // collect the contributions of the
      // subfaces and store them with the
      // mother face
      std::vector<double> sum (n_solution_vectors, 0);
      for (unsigned int subface_no=0; subface_no<face->n_children(); ++subface_no)
        {
          Assert (local_face_integrals.find(face->child(subface_no)) !=
                  local_face_integrals.end(),
                  ExcInternalError());
          Assert (local_face_integrals[face->child(subface_no)][0] >= 0,
                  ExcInternalError());

          for (unsigned int n=0; n<n_solution_vectors; ++n)
            sum[n] += local_face_integrals[face->child(subface_no)][n];
        }

      local_face_integrals[face] = sum;
    }


    /**
     * Computate the error on the faces of a
     * single cell.
     *
     * This function is only needed
     * in two or three dimensions.
     * The error estimator in one
     * dimension is implemented
     * seperatly.
     */
    template <int dim, int spacedim, typename InputVector, class DH>
    void
    estimate_one_cell (const typename DH::active_cell_iterator &cell,
                       ParallelData<DH>                    &parallel_data,
                       std::map<typename DH::face_iterator,std::vector<double> > &local_face_integrals,
                       const std::vector<const InputVector *> &solutions)
    {
      const unsigned int n_solution_vectors = solutions.size();

      const types::subdomain_id subdomain_id = parallel_data.subdomain_id;
      const unsigned int material_id  = parallel_data.material_id;

      // empty our own copy of the local face
      // integrals
      local_face_integrals.clear();

      // loop over all faces of this cell
      for (unsigned int face_no=0;
           face_no<GeometryInfo<dim>::faces_per_cell; ++face_no)
        {
          const typename DH::face_iterator
          face=cell->face(face_no);

          // make sure we do work
          // only once: this face
          // may either be regular
          // or irregular. if it is
          // regular and has a
          // neighbor, then we
          // visit the face twice,
          // once from every
          // side. let the one with
          // the lower index do the
          // work. if it is at the
          // boundary, or if the
          // face is irregular,
          // then do the work below
          if ((face->has_children() == false) &&
              !cell->at_boundary(face_no) &&
              (!cell->neighbor_is_coarser(face_no) &&
               (cell->neighbor(face_no)->index() < cell->index() ||
                (cell->neighbor(face_no)->index() == cell->index() &&
                 cell->neighbor(face_no)->level() < cell->level()))))
            continue;

          // if the neighboring cell is less
          // refined than the present one,
          // then do nothing since we
          // integrate over the subfaces when
          // we visit the coarse cells.
          if (face->at_boundary() == false)
            if (cell->neighbor_is_coarser(face_no))
              continue;

          // if this face is part of the
          // boundary but not of the neumann
          // boundary -> nothing to
          // do. However, to make things
          // easier when summing up the
          // contributions of the faces of
          // cells, we enter this face into
          // the list of faces with
          // contribution zero.
          if (face->at_boundary()
              &&
              (parallel_data.neumann_bc->find(face->boundary_indicator()) ==
               parallel_data.neumann_bc->end()))
            {
              local_face_integrals[face]
                = std::vector<double> (n_solution_vectors, 0.);
              continue;
            }

          // finally: note that we only have
          // to do something if either the
          // present cell is on the subdomain
          // we care for (and the same for
          // material_id), or if one of the
          // neighbors behind the face is on
          // the subdomain we care for
          if ( ! ( ((subdomain_id == numbers::invalid_subdomain_id)
                    ||
                    (cell->subdomain_id() == subdomain_id))
                   &&
                   ((material_id == numbers::invalid_material_id)
                    ||
                    (cell->material_id() == material_id))) )
            {
              // ok, cell is unwanted, but
              // maybe its neighbor behind
              // the face we presently work
              // on? oh is there a face at
              // all?
              if (face->at_boundary())
                continue;

              bool care_for_cell = false;
              if (face->has_children() == false)
                care_for_cell |= ((cell->neighbor(face_no)->subdomain_id()
                                   == subdomain_id) ||
                                  (subdomain_id == numbers::invalid_subdomain_id))
                                 &&
                                 ((cell->neighbor(face_no)->material_id()
                                   == material_id) ||
                                  (material_id == numbers::invalid_material_id));
              else
                {
                  for (unsigned int sf=0; sf<face->n_children(); ++sf)
                    if (((cell->neighbor_child_on_subface(face_no,sf)
                          ->subdomain_id() == subdomain_id)
                         &&
                         (material_id ==
                          numbers::invalid_material_id))
                        ||
                        ((cell->neighbor_child_on_subface(face_no,sf)
                          ->material_id() == material_id)
                         &&
                         (subdomain_id ==
                          numbers::invalid_subdomain_id)))
                      {
                        care_for_cell = true;
                        break;
                      }
                }

              // so if none of the neighbors
              // cares for this subdomain or
              // material either, then try
              // next face
              if (care_for_cell == false)
                continue;
            }

          // so now we know that we care for
          // this face, let's do something
          // about it. first re-size the
          // arrays we may use to the correct
          // size:
          parallel_data.resize (cell->active_fe_index());


          // then do the actual integration
          if (face->has_children() == false)
            // if the face is a regular one,
            // i.e.  either on the other side
            // there is nirvana (face is at
            // boundary), or the other side's
            // refinement level is the same
            // as that of this side, then
            // handle the integration of
            // these both cases together
            integrate_over_regular_face (solutions,
                                         parallel_data,
                                         local_face_integrals,
                                         cell, face_no,
                                         parallel_data.fe_face_values_cell,
                                         parallel_data.fe_face_values_neighbor);

          else
            // otherwise we need to do some
            // special computations which do
            // not fit into the framework of
            // the above function
            integrate_over_irregular_face (solutions,
                                           parallel_data,
                                           local_face_integrals,
                                           cell, face_no,
                                           parallel_data.fe_face_values_cell,
                                           parallel_data.fe_subface_values);
        }
    }
  }
}







template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const Mapping<1,spacedim>      &mapping,
          const DH   &dof_handler,
          const Quadrature<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  // just pass on to the other function
  const std::vector<const InputVector *> solutions (1, &solution);
  std::vector<Vector<float>*>              errors (1, &error);
  estimate (mapping, dof_handler, quadrature, neumann_bc, solutions, errors,
            component_mask, coefficients, n_threads, subdomain_id, material_id);
}



template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const DH   &dof_handler,
          const Quadrature<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<1,spacedim>::mapping, dof_handler, quadrature, neumann_bc, solution,
           error, component_mask, coefficients, n_threads, subdomain_id, material_id);
}



template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const DH   &dof_handler,
          const Quadrature<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const std::vector<const InputVector *> &solutions,
          std::vector<Vector<float>*> &errors,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<1,spacedim>::mapping, dof_handler, quadrature, neumann_bc, solutions,
           errors, component_mask, coefficients, n_threads, subdomain_id, material_id);
}



template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const Mapping<1,spacedim>      &mapping,
          const DH   &dof_handler,
          const hp::QCollection<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  // just pass on to the other function
  const std::vector<const InputVector *> solutions (1, &solution);
  std::vector<Vector<float>*>              errors (1, &error);
  estimate (mapping, dof_handler, quadrature, neumann_bc, solutions, errors,
            component_mask, coefficients, n_threads, subdomain_id, material_id);
}


template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const DH   &dof_handler,
          const hp::QCollection<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<1,spacedim>::mapping, dof_handler, quadrature, neumann_bc, solution,
           error, component_mask, coefficients, n_threads, subdomain_id, material_id);
}



template <int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<1,spacedim>::
estimate (const DH   &dof_handler,
          const hp::QCollection<0> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const std::vector<const InputVector *> &solutions,
          std::vector<Vector<float>*> &errors,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<1,spacedim>::mapping, dof_handler, quadrature, neumann_bc, solutions,
           errors, component_mask, coefficients, n_threads, subdomain_id, material_id);
}




template <int spacedim>
template <typename InputVector, class DH>
void KellyErrorEstimator<1,spacedim>::
estimate (const Mapping<1,spacedim>                    &/*mapping*/,
          const DH                            &/*dof_handler*/,
          const hp::QCollection<0> &,
          const typename FunctionMap<spacedim>::type          &/*neumann_bc*/,
          const std::vector<const InputVector *> &/*solutions*/,
          std::vector<Vector<float>*>            &/*errors*/,
          const ComponentMask                &/*component_mask_*/,
          const Function<spacedim>                   */*coefficient*/,
          const unsigned int,
          const types::subdomain_id          /*subdomain_id*/,
          const types::material_id                   /*material_id*/)
{
  Assert (false, ExcInternalError());
}



template <int spacedim>
template <typename InputVector, class DH>
void KellyErrorEstimator<1,spacedim>::
estimate (const Mapping<1,spacedim>                    &mapping,
          const DH                 &dof_handler,
          const Quadrature<0> &,
          const typename FunctionMap<spacedim>::type          &neumann_bc,
          const std::vector<const InputVector *> &solutions,
          std::vector<Vector<float>*>              &errors,
          const ComponentMask                  &component_mask,
          const Function<spacedim>                   *coefficient,
          const unsigned int,
          const types::subdomain_id         subdomain_id_,
          const types::material_id                  material_id)
{
#ifdef DEAL_II_WITH_P4EST
  if (dynamic_cast<const parallel::distributed::Triangulation<1,spacedim>*>
      (&dof_handler.get_tria())
      != 0)
    Assert ((subdomain_id_ == numbers::invalid_subdomain_id)
            ||
            (subdomain_id_ ==
             dynamic_cast<const parallel::distributed::Triangulation<1,spacedim>&>
             (dof_handler.get_tria()).locally_owned_subdomain()),
            ExcMessage ("For parallel distributed triangulations, the only "
                        "valid subdomain_id that can be passed here is the "
                        "one that corresponds to the locally owned subdomain id."));

  const types::subdomain_id subdomain_id
    = ((dynamic_cast<const parallel::distributed::Triangulation<1,spacedim>*>
        (&dof_handler.get_tria())
        != 0)
       ?
       dynamic_cast<const parallel::distributed::Triangulation<1,spacedim>&>
       (dof_handler.get_tria()).locally_owned_subdomain()
       :
       subdomain_id_);
#else
  const types::subdomain_id subdomain_id
    = subdomain_id_;
#endif

  const unsigned int n_components       = dof_handler.get_fe().n_components();
  const unsigned int n_solution_vectors = solutions.size();

  // sanity checks
  Assert (neumann_bc.find(numbers::internal_face_boundary_id) == neumann_bc.end(),
          ExcInvalidBoundaryIndicator());

  for (typename FunctionMap<spacedim>::type::const_iterator i=neumann_bc.begin();
       i!=neumann_bc.end(); ++i)
    Assert (i->second->n_components == n_components, ExcInvalidBoundaryFunction());

  Assert (component_mask.represents_n_components(n_components),
          ExcInvalidComponentMask());
  Assert (component_mask.n_selected_components(n_components) > 0,
          ExcInvalidComponentMask());

  Assert ((coefficient == 0) ||
          (coefficient->n_components == n_components) ||
          (coefficient->n_components == 1),
          ExcInvalidCoefficient());

  Assert (solutions.size() > 0,
          ExcNoSolutions());
  Assert (solutions.size() == errors.size(),
          ExcIncompatibleNumberOfElements(solutions.size(), errors.size()));
  for (unsigned int n=0; n<solutions.size(); ++n)
    Assert (solutions[n]->size() == dof_handler.n_dofs(),
            ExcInvalidSolutionVector());

  Assert ((coefficient == 0) ||
          (coefficient->n_components == n_components) ||
          (coefficient->n_components == 1),
          ExcInvalidCoefficient());

  for (typename FunctionMap<spacedim>::type::const_iterator i=neumann_bc.begin();
       i!=neumann_bc.end(); ++i)
    Assert (i->second->n_components == n_components,
            ExcInvalidBoundaryFunction());

  // reserve one slot for each cell and set
  // it to zero
  for (unsigned int n=0; n<n_solution_vectors; ++n)
    (*errors[n]).reinit (dof_handler.get_tria().n_active_cells());

  // fields to get the gradients on
  // the present and the neighbor cell.
  //
  // for the neighbor gradient, we
  // need several auxiliary fields,
  // depending on the way we get it
  // (see below)
  std::vector<std::vector<std::vector<Tensor<1,spacedim> > > >
  gradients_here (n_solution_vectors,
                  std::vector<std::vector<Tensor<1,spacedim> > >(2, std::vector<Tensor<1,spacedim> >(n_components)));
  std::vector<std::vector<std::vector<Tensor<1,spacedim> > > >
  gradients_neighbor (gradients_here);
  std::vector<Vector<double> >
  grad_neighbor (n_solution_vectors, Vector<double>(n_components));

  // reserve some space for
  // coefficient values at one point.
  // if there is no coefficient, then
  // we fill it by unity once and for
  // all and don't set it any more
  Vector<double> coefficient_values (n_components);
  if (coefficient == 0)
    for (unsigned int c=0; c<n_components; ++c)
      coefficient_values(c) = 1;

  const QTrapez<1> quadrature;
  const hp::QCollection<1> q_collection(quadrature);

  const hp::FECollection<1,spacedim> fe (dof_handler.get_fe());

  hp::MappingCollection<1,spacedim> mapping_collection;
  mapping_collection.push_back (mapping);

  hp::FEValues<1,spacedim> fe_values (mapping_collection, fe, q_collection,
                                      update_gradients);

  // loop over all cells and do something on
  // the cells which we're told to work
  // on. note that the error indicator is
  // only a sum over the two contributions
  // from the two vertices of each cell.
  typename DH::active_cell_iterator cell = dof_handler.begin_active();
  for (unsigned int cell_index=0; cell != dof_handler.end();
       ++cell, ++cell_index)
    if (((subdomain_id == numbers::invalid_subdomain_id)
         ||
         (cell->subdomain_id() == subdomain_id))
        &&
        ((material_id == numbers::invalid_material_id)
         ||
         (cell->material_id() == material_id)))
      {
        for (unsigned int n=0; n<n_solution_vectors; ++n)
          (*errors[n])(cell_index) = 0;

        // loop over the two points bounding
        // this line. n==0 is left point,
        // n==1 is right point
        for (unsigned int n=0; n<2; ++n)
          {
            // find left or right active
            // neighbor
            typename DH::cell_iterator neighbor = cell->neighbor(n);
            if (neighbor.state() == IteratorState::valid)
              while (neighbor->has_children())
                neighbor = neighbor->child(n==0 ? 1 : 0);

            // now get the gradients on the
            // both sides of the point
            fe_values.reinit (cell);

            for (unsigned int s=0; s<n_solution_vectors; ++s)
              fe_values.get_present_fe_values()
              .get_function_gradients (*solutions[s], gradients_here[s]);

            if (neighbor.state() == IteratorState::valid)
              {
                fe_values.reinit (neighbor);

                for (unsigned int s=0; s<n_solution_vectors; ++s)
                  fe_values.get_present_fe_values()
                  .get_function_gradients (*solutions[s],
                                           gradients_neighbor[s]);

                // extract the
                // gradients of all the
                // components. [0]
                // means: x-derivative,
                // which is the only
                // one here
                for (unsigned int s=0; s<n_solution_vectors; ++s)
                  for (unsigned int c=0; c<n_components; ++c)
                    grad_neighbor[s](c)
                      = gradients_neighbor[s][n==0 ? 1 : 0][c][0];
              }
            else if (neumann_bc.find(n) != neumann_bc.end())
              // if Neumann b.c., then fill
              // the gradients field which
              // will be used later on.
              {
                if (n_components==1)
                  {
                    const double
                    v = neumann_bc.find(n)->second->value(cell->vertex(0));

                    for (unsigned int s=0; s<n_solution_vectors; ++s)
                      grad_neighbor[s](0) = v;
                  }
                else
                  {
                    Vector<double> v(n_components);
                    neumann_bc.find(n)->second->vector_value(cell->vertex(0), v);

                    for (unsigned int s=0; s<n_solution_vectors; ++s)
                      grad_neighbor[s] = v;
                  }
              }
            else
              // fill with zeroes.
              for (unsigned int s=0; s<n_solution_vectors; ++s)
                grad_neighbor[s] = 0;

            // if there is a
            // coefficient, then
            // evaluate it at the
            // present position. if
            // there is none, reuse the
            // preset values.
            if (coefficient != 0)
              {
                if (coefficient->n_components == 1)
                  {
                    const double c_value = coefficient->value (cell->vertex(n));
                    for (unsigned int c=0; c<n_components; ++c)
                      coefficient_values(c) = c_value;
                  }
                else
                  coefficient->vector_value(cell->vertex(n),
                                            coefficient_values);
              }


            for (unsigned int s=0; s<n_solution_vectors; ++s)
              for (unsigned int component=0; component<n_components; ++component)
                if (component_mask[component] == true)
                  {
                    // get gradient
                    // here. [0] means
                    // x-derivative
                    // (there is no
                    // other component
                    // in 1d)
                    const double grad_here = gradients_here[s][n][component][0];

                    const double jump = ((grad_here - grad_neighbor[s](component)) *
                                         coefficient_values(component));
                    (*errors[s])(cell_index) += jump*jump * cell->diameter();
                  }
          }

        for (unsigned int s=0; s<n_solution_vectors; ++s)
          (*errors[s])(cell_index) = std::sqrt((*errors[s])(cell_index));
      }
}






// the following function is still independent of dimension, but it
// calls dimension dependent functions
template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim, spacedim>::
estimate (const Mapping<dim, spacedim>      &mapping,
          const DH                &dof_handler,
          const Quadrature<dim-1> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  // just pass on to the other function
  const std::vector<const InputVector *> solutions (1, &solution);
  std::vector<Vector<float>*>              errors (1, &error);
  estimate (mapping, dof_handler, quadrature, neumann_bc, solutions, errors,
            component_mask, coefficients, n_threads, subdomain_id, material_id);
}


template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim,spacedim>::
estimate (const DH                &dof_handler,
          const Quadrature<dim-1> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<dim,spacedim>::mapping, dof_handler, quadrature, neumann_bc, solution,
           error, component_mask, coefficients, n_threads,
           subdomain_id, material_id);
}


template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim, spacedim>::
estimate (const Mapping<dim, spacedim>      &mapping,
          const DH                &dof_handler,
          const hp::QCollection<dim-1> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  // just pass on to the other function
  const std::vector<const InputVector *> solutions (1, &solution);
  std::vector<Vector<float>*>              errors (1, &error);
  estimate (mapping, dof_handler, quadrature, neumann_bc, solutions, errors,
            component_mask, coefficients, n_threads, subdomain_id, material_id);
}


template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim, spacedim>::
estimate (const DH                &dof_handler,
          const hp::QCollection<dim-1> &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const InputVector       &solution,
          Vector<float>           &error,
          const ComponentMask &component_mask,
          const Function<spacedim>     *coefficients,
          const unsigned int       n_threads,
          const types::subdomain_id subdomain_id,
          const types::material_id       material_id)
{
  estimate(StaticMappingQ1<dim, spacedim>::mapping, dof_handler, quadrature, neumann_bc, solution,
           error, component_mask, coefficients, n_threads,
           subdomain_id, material_id);
}




template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim, spacedim>::
estimate (const Mapping<dim, spacedim>                  &mapping,
          const DH                            &dof_handler,
          const hp::QCollection<dim-1>        &face_quadratures,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const std::vector<const InputVector *> &solutions,
          std::vector<Vector<float>*>              &errors,
          const ComponentMask                  &component_mask,
          const Function<spacedim>                 *coefficients,
          const unsigned int                   ,
          const types::subdomain_id          subdomain_id_,
          const types::material_id                   material_id)
{
#ifdef DEAL_II_WITH_P4EST
  if (dynamic_cast<const parallel::distributed::Triangulation<dim,spacedim>*>
      (&dof_handler.get_tria())
      != 0)
    Assert ((subdomain_id_ == numbers::invalid_subdomain_id)
            ||
            (subdomain_id_ ==
             dynamic_cast<const parallel::distributed::Triangulation<dim,spacedim>&>
             (dof_handler.get_tria()).locally_owned_subdomain()),
            ExcMessage ("For parallel distributed triangulations, the only "
                        "valid subdomain_id that can be passed here is the "
                        "one that corresponds to the locally owned subdomain id."));

  const types::subdomain_id subdomain_id
    = ((dynamic_cast<const parallel::distributed::Triangulation<dim,spacedim>*>
        (&dof_handler.get_tria())
        != 0)
       ?
       dynamic_cast<const parallel::distributed::Triangulation<dim,spacedim>&>
       (dof_handler.get_tria()).locally_owned_subdomain()
       :
       subdomain_id_);
#else
  const types::subdomain_id subdomain_id
    = subdomain_id_;
#endif

  const unsigned int n_components = dof_handler.get_fe().n_components();

  // sanity checks
  Assert (solutions.size() > 0,
          ExcNoSolutions());
  Assert (solutions.size() == errors.size(),
          ExcIncompatibleNumberOfElements(solutions.size(), errors.size()));

  for (typename FunctionMap<spacedim>::type::const_iterator i=neumann_bc.begin();
       i!=neumann_bc.end(); ++i)
    Assert (i->second->n_components == n_components,
            ExcInvalidBoundaryFunction());

  Assert (component_mask.represents_n_components(n_components),
          ExcInvalidComponentMask());
  Assert (component_mask.n_selected_components(n_components) > 0,
          ExcInvalidComponentMask());

  Assert ((coefficients == 0) ||
          (coefficients->n_components == n_components) ||
          (coefficients->n_components == 1),
          ExcInvalidCoefficient());

  for (unsigned int n=0; n<solutions.size(); ++n)
    Assert (solutions[n]->size() == dof_handler.n_dofs(),
            ExcInvalidSolutionVector());

  const unsigned int n_solution_vectors = solutions.size();

  // Map of integrals indexed by
  // the corresponding face. In this map
  // we store the integrated jump of the
  // gradient for each face.
  // At the end of the function, we again
  // loop over the cells and collect the
  // contributions of the different faces
  // of the cell.
  std::map<typename DH::face_iterator,std::vector<double> > face_integrals;

  // all the data needed in the error
  // estimator by each of the threads
  // is gathered in the following
  // stuctures
  const hp::MappingCollection<dim,spacedim> mapping_collection(mapping);
  const internal::ParallelData<DH>
  parallel_data (dof_handler.get_fe(),
                 face_quadratures,
                 mapping_collection,
                 (!neumann_bc.empty() || (coefficients != 0)),
                 solutions.size(),
                 subdomain_id,
                 material_id,
                 &neumann_bc,
                 component_mask,
                 coefficients);
  std::map<typename DH::face_iterator,std::vector<double> > sample_local_face_integrals;

  // now let's work on all those cells:
  WorkStream::run (dof_handler.begin_active(),
                   static_cast<typename DH::active_cell_iterator>(dof_handler.end()),
                   std_cxx1x::bind (&internal::estimate_one_cell<dim,spacedim,InputVector,DH>,
                                    std_cxx1x::_1, std_cxx1x::_2, std_cxx1x::_3, std_cxx1x::ref(solutions)),
                   std_cxx1x::bind (&internal::copy_local_to_global<DH>,
                                    std_cxx1x::_1, std_cxx1x::ref(face_integrals)),
                   parallel_data,
                   sample_local_face_integrals);

  // finally add up the contributions of the
  // faces for each cell

  // reserve one slot for each cell and set
  // it to zero
  for (unsigned int n=0; n<n_solution_vectors; ++n)
    {
      (*errors[n]).reinit (dof_handler.get_tria().n_active_cells());
      for (unsigned int i=0; i<dof_handler.get_tria().n_active_cells(); ++i)
        (*errors[n])(i)=0;
    }

  // now walk over all cells and collect
  // information from the faces. only do
  // something if this is a cell we care for
  // based on the subdomain id
  unsigned int present_cell=0;
  for (typename DH::active_cell_iterator cell=dof_handler.begin_active();
       cell!=dof_handler.end();
       ++cell, ++present_cell)
    if ( ((subdomain_id == numbers::invalid_subdomain_id)
          ||
          (cell->subdomain_id() == subdomain_id))
         &&
         ((material_id == numbers::invalid_material_id)
          ||
          (cell->material_id() == material_id)))
      {
        // loop over all faces of this cell
        for (unsigned int face_no=0; face_no<GeometryInfo<dim>::faces_per_cell;
             ++face_no)
          {
            Assert(face_integrals.find(cell->face(face_no))
                   != face_integrals.end(),
                   ExcInternalError());
            const double factor = cell->diameter() / 24;

            for (unsigned int n=0; n<n_solution_vectors; ++n)
              {
                // make sure that we have
                // written a meaningful value
                // into this slot
                Assert (face_integrals[cell->face(face_no)][n] >= 0,
                        ExcInternalError());

                (*errors[n])(present_cell)
                += (face_integrals[cell->face(face_no)][n] * factor);
              }
          }

        for (unsigned int n=0; n<n_solution_vectors; ++n)
          (*errors[n])(present_cell) = std::sqrt((*errors[n])(present_cell));
      }
}



template <int dim, int spacedim>
template <typename InputVector, class DH>
void
KellyErrorEstimator<dim, spacedim>::
estimate (const Mapping<dim, spacedim>                  &mapping,
          const DH                            &dof_handler,
          const Quadrature<dim-1>             &quadrature,
          const typename FunctionMap<spacedim>::type &neumann_bc,
          const std::vector<const InputVector *> &solutions,
          std::vector<Vector<float>*>              &errors,
          const ComponentMask                  &component_mask,
          const Function<spacedim>                 *coefficients,
          const unsigned int                   n_threads,
          const types::subdomain_id          subdomain_id,
          const types::material_id                   material_id)
{
  // forward to the function with the QCollection
  estimate (mapping, dof_handler,
            hp::QCollection<dim-1>(quadrature),
            neumann_bc, solutions,
            errors, component_mask, coefficients,
            n_threads, subdomain_id, material_id);
}


template <int dim, int spacedim>
template <typename InputVector, class DH>
void KellyErrorEstimator<dim, spacedim>::estimate (const DH                            &dof_handler,
                                                   const Quadrature<dim-1>             &quadrature,
                                                   const typename FunctionMap<spacedim>::type &neumann_bc,
                                                   const std::vector<const InputVector *> &solutions,
                                                   std::vector<Vector<float>*>              &errors,
                                                   const ComponentMask                  &component_mask,
                                                   const Function<spacedim>                 *coefficients,
                                                   const unsigned int                   n_threads,
                                                   const types::subdomain_id subdomain_id,
                                                   const types::material_id       material_id)
{
  estimate(StaticMappingQ1<dim, spacedim>::mapping, dof_handler, quadrature, neumann_bc, solutions,
           errors, component_mask, coefficients, n_threads,
           subdomain_id, material_id);
}



template <int dim, int spacedim>
template <typename InputVector, class DH>
void KellyErrorEstimator<dim, spacedim>::estimate (const DH                            &dof_handler,
                                                   const hp::QCollection<dim-1>             &quadrature,
                                                   const typename FunctionMap<spacedim>::type &neumann_bc,
                                                   const std::vector<const InputVector *> &solutions,
                                                   std::vector<Vector<float>*>              &errors,
                                                   const ComponentMask                  &component_mask,
                                                   const Function<spacedim>                 *coefficients,
                                                   const unsigned int                   n_threads,
                                                   const types::subdomain_id subdomain_id,
                                                   const types::material_id       material_id)
{
  estimate(StaticMappingQ1<dim, spacedim>::mapping, dof_handler, quadrature, neumann_bc, solutions,
           errors, component_mask, coefficients, n_threads,
           subdomain_id, material_id);
}






// explicit instantiations
#include "error_estimator.inst"


DEAL_II_NAMESPACE_CLOSE
