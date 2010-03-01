// $Id$
/*****************************************************************************
 *   Copyright (C) 2009 by Klaus Mosthaf                                     *
 *   Copyright (C) 2009 by Andreas Lauser                                    *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version, as long as this copyright notice    *
 *   is included in its original form.                                       *
 *                                                                           *
 *   This program is distributed WITHOUT ANY WARRANTY.                       *
 *****************************************************************************/
#ifndef DUNE_WATERAIRPROBLEM_HH
#define DUNE_WATERAIRPROBLEM_HH

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dune/grid/io/file/dgfparser/dgfug.hh>
#include <dune/grid/io/file/dgfparser/dgfs.hh>
#include <dune/grid/io/file/dgfparser/dgfyasp.hh>

#include <dumux/new_material/fluidsystems/h2o_n2_system.hh>

#include <dumux/boxmodels/2p2cni/2p2cniboxmodel.hh>

#include "waterairspatialparameters.hh"

#define ISOTHERMAL 0

/*!
 * \ingroup BoxProblems
 * \brief TwoPTwoCNIBoxProblems Non-isothermal two-phase two-component box problems
 */

namespace Dune
{
template <class TypeTag>
class WaterAirProblem;

namespace Properties
{
NEW_TYPE_TAG(WaterAirProblem, INHERITS_FROM(BoxTwoPTwoCNI));

// Set the grid type
SET_PROP(WaterAirProblem, Grid)
{
    typedef Dune::YaspGrid<2> type;
};

#ifdef HAVE_DUNE_PDELAB
SET_PROP(WaterAirProblem, LocalFEMSpace)
{
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar)) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView)) GridView;
    enum{dim = GridView::dimension};

public:
    typedef Dune::PDELab::Q1LocalFiniteElementMap<Scalar,Scalar,dim>  type; // for cubes
//    typedef Dune::PDELab::P1LocalFiniteElementMap<Scalar,Scalar,dim>  type; // for simplices
};
#endif

// Set the problem property
SET_PROP(WaterAirProblem, Problem)
{
    typedef Dune::WaterAirProblem<TTAG(WaterAirProblem)> type;
};

// Set the wetting phase
SET_TYPE_PROP(WaterAirProblem, FluidSystem, Dune::H2O_N2_System<TypeTag>);

// Set the soil properties
SET_TYPE_PROP(WaterAirProblem, 
              SpatialParameters,
              Dune::WaterAirSpatialParameters<TypeTag>);

// Enable gravity
SET_BOOL_PROP(WaterAirProblem, EnableGravity, true);

// Write newton convergence
SET_BOOL_PROP(WaterAirProblem, NewtonWriteConvergence, true);
}


/*!
 * \ingroup TwoPTwoCNIBoxProblems
 * \brief Nonisothermal gas injection problem where a gas (e.g. air) is injected into a fully
 *        water saturated medium. During buoyancy driven upward migration the gas
 *        passes a high temperature area.
 *
 * The domain is sized 40 m times 40 m. The rectangular area with the increased temperature (380 K)
 * starts at (20 m, 5 m) and ends at (30 m, 35 m).
 *
 * For the mass conservation equation neumann boundary conditions are used on
 * the top and on the bottom of the domain, while dirichlet conditions
 * apply on the left and the right boundary.
 * For the energy conservation equation dirichlet boundary conditions are applied
 * on all boundaries.
 *
 * Gas is injected at the bottom boundary from 15 m to 25 m at a rate of
 * 0.001 kg/(s m), the remaining neumann boundaries are no-flow
 * boundaries.
 *
 * At the dirichlet boundaries a hydrostatic pressure, a gas saturation of zero and
 * a geothermal temperature gradient of 0.03 K/m are applied.
 *
 * This problem uses the \ref TwoPTwoCNIBoxModel.
 *
 * This problem should typically be simulated for 300000 s.
 * A good choice for the initial time step size is 1000 s.
 *
 * To run the simulation execute the following line in shell: 
 * <tt>./test_2p2cni ./grids/test_2p2cni.dgf 300000 1000</tt>
 *  */
template <class TypeTag = TTAG(WaterAirProblem) >
class WaterAirProblem : public TwoPTwoCNIBoxProblem<TypeTag, WaterAirProblem<TypeTag> >
{
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar))     Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView))   GridView;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Model))      Model;
    typedef typename GridView::Grid                           Grid;

    typedef WaterAirProblem<TypeTag>                ThisType;
    typedef TwoPTwoCNIBoxProblem<TypeTag, ThisType> ParentType;

    // copy some indices for convenience
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(TwoPTwoCIndices)) Indices;
    enum {
        numEq       = GET_PROP_VALUE(TypeTag, PTAG(NumEq)),

        pressureIdx = Indices::pressureIdx,
        switchIdx   = Indices::switchIdx,
#if !ISOTHERMAL
        temperatureIdx = Indices::temperatureIdx,
#endif

        // Phase State
        lPhaseOnly  = Indices::lPhaseOnly,
        gPhaseOnly  = Indices::gPhaseOnly,
        bothPhases  = Indices::bothPhases,

        // Grid and world dimension
        dim         = GridView::dimension,
        dimWorld    = GridView::dimensionworld,
    };

    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes)) SolutionTypes;
    typedef typename SolutionTypes::PrimaryVarVector        PrimaryVarVector;
    typedef typename SolutionTypes::BoundaryTypeVector      BoundaryTypeVector;

    typedef typename GridView::template Codim<0>::Entity        Element;
    typedef typename GridView::template Codim<dim>::Entity      Vertex;
    typedef typename GridView::Intersection                     Intersection;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FVElementGeometry)) FVElementGeometry;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FluidSystem)) FluidSystem;

    typedef Dune::FieldVector<Scalar, dim>       LocalPosition;
    typedef Dune::FieldVector<Scalar, dimWorld>  GlobalPosition;

public:
    WaterAirProblem(const GridView &gridView)
        : ParentType(gridView)
    {
        FluidSystem::init();
    }

    /*!
     * \name Problem parameters
     */
    // \{

    /*!
     * \brief The problem name.
     *
     * This is used as a prefix for files generated by the simulation.
     */
    const char *name() const
    { return "waterair"; }

#if ISOTHERMAL
    /*!
     * \brief Returns the temperature within the domain.
     *
     * This problem assumes a temperature of 10 degrees Celsius.
     */
    Scalar temperature(const Element           &element,
                       const FVElementGeometry &fvElemGeom,
                       int                      scvIdx) const
    {
        return 273.15 + 10; // -> 10°C
    };
#endif

    // \}

    /*!
     * \name Boundary conditions
     */
    // \{

    /*!
     * \brief Specifies which kind of boundary condition should be
     *        used for which equation on a given boundary segment.
     */
    void boundaryTypes(BoundaryTypeVector         &values,
                       const Element              &element,
                       const FVElementGeometry    &fvElemGeom,
                       const Intersection         &is,
                       int                         scvIdx,
                       int                         boundaryFaceIdx) const
    {
        const GlobalPosition &globalPos  = element.geometry().corner(scvIdx);

        if(globalPos[0] > 40 - eps_ || globalPos[0] < eps_)
            values = BoundaryConditions::dirichlet;
        else
            values = BoundaryConditions::neumann;

#if !ISOTHERMAL
        values[temperatureIdx] = BoundaryConditions::dirichlet;
#endif
    }

    /*!
     * \brief Evaluate the boundary conditions for a dirichlet
     *        boundary segment.
     *
     * For this method, the \a values parameter stores primary variables.
     */
    void dirichlet(PrimaryVarVector           &values,
                   const Element              &element,
                   const FVElementGeometry    &fvElemGeom,
                   const Intersection         &is,
                   int                         scvIdx,
                   int                         boundaryFaceIdx) const
    {
        const GlobalPosition &globalPos
            = element.geometry().corner(scvIdx);
        //                const LocalPosition &localPos
        //                    = DomainTraits::referenceElement(element.geometry().type()).position(dim,scvIdx);

        initial_(values, globalPos);
    }

    /*!
     * \brief Evaluate the boundary conditions for a neumann
     *        boundary segment.
     *
     * For this method, the \a values parameter stores the mass flux
     * in normal direction of each component. Negative values mean
     * influx.
     */
    void neumann(PrimaryVarVector           &values,
                 const Element              &element,
                 const FVElementGeometry    &fvElemGeom,
                 const Intersection         &is,
                 int                         scvIdx,
                 int                         boundaryFaceIdx) const
    {
        const GlobalPosition &globalPos = element.geometry().corner(scvIdx);
        values = 0;

        // negative values for injection
        if (globalPos[0] > 15 && globalPos[0] < 25 &&
            globalPos[1] < eps_)
        {
            values[Indices::contiGEqIdx] = -1e-3;
        }
    }

    // \}

    /*!
     * \name Volume terms
     */
    // \{

    /*!
     * \brief Evaluate the source term for all phases within a given
     *        sub-control-volume.
     *
     * For this method, the \a values parameter stores the rate mass
     * of a component is generated or annihilate per volume
     * unit. Positive values mean that mass is created, negative ones
     * mean that it vanishes.
     */
    void source(PrimaryVarVector        &values,
                const Element           &element,
                const FVElementGeometry &fvElemGeom,
                int                      scvIdx) const
    {
	       values = Scalar(0.0);
    }

    /*!
     * \brief Evaluate the initial value for a control volume.
     *
     * For this method, the \a values parameter stores primary
     * variables.
     */
    void initial(PrimaryVarVector        &values,
                 const Element           &element,
                 const FVElementGeometry &fvElemGeom,
                 int                      scvIdx) const
    {
	       const GlobalPosition &globalPos = element.geometry().corner(scvIdx);

        initial_(values, globalPos);

#if !ISOTHERMAL
 	       if (globalPos[0] > 20 && globalPos[0] < 30 && globalPos[1] < 30)
	    	   values[temperatureIdx] = 380;
#endif
    }

    /*!
     * \brief Return the initial phase state inside a control volume.
     */
    int initialPhasePresence(const Vertex         &vert,
                             int                  &globalIdx,
                             const GlobalPosition &globalPos) const
    {
        return lPhaseOnly;
    }

private:
    // internal method for the initial condition (reused for the
    // dirichlet conditions!)
    void initial_(PrimaryVarVector     &values,
                  const GlobalPosition &globalPos) const
    {
        Scalar densityW = 1000.0;
        values[pressureIdx] = 1e5 + (depthBOR_ - globalPos[1])*densityW*9.81;
        values[switchIdx] = 0.0;
#if !ISOTHERMAL
        values[temperatureIdx] = 283.0 + (depthBOR_ - globalPos[1])*0.03;
#endif
    }

    static const Scalar depthBOR_ = 1000.0; // [m]
    static const Scalar eps_ = 1e-6;
};
} //end namespace

#endif
