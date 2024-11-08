/*********************************************************************************
*  This file is part of reference implementation of SIGGRAPH Asia 2023 Paper     *
*  `Metric Optimization in Penner Coordinates`           *
*  v1.0                                                                          *
*                                                                                *
*  The MIT License                                                               *
*                                                                                *
*  Permission is hereby granted, free of charge, to any person obtaining a       *
*  copy of this software and associated documentation files (the "Software"),    *
*  to deal in the Software without restriction, including without limitation     *
*  the rights to use, copy, modify, merge, publish, distribute, sublicense,      *
*  and/or sell copies of the Software, and to permit persons to whom the         *
*  Software is furnished to do so, subject to the following conditions:          *
*                                                                                *
*  The above copyright notice and this permission notice shall be included in    *
*  all copies or substantial portions of the Software.                           *
*                                                                                *
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    *
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      *
*  FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE  *
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        *
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING       *
*  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS  *
*  IN THE SOFTWARE.                                                              *
*                                                                                *
*  Author(s):                                                                    *
*  Ryan Capouellez, Denis Zorin,                                                 *
*  Courant Institute of Mathematical Sciences, New York University, USA          *
*                                          *                                     *
*********************************************************************************/
#include "optimization/core/common.h"
#include "optimization/metric_optimization/explicit_optimization.h"
#include "optimization/metric_optimization/energies.h"
#include "optimization/interface.h"
#include "optimization/core/constraint.h"
#include "optimization/core/shear.h"
#include "optimization/core/projection.h"
#include "util/io.h"
#include <igl/readOBJ.h>
#include <igl/writeOBJ.h>

/// Unlike with Penner coordinates, any choice of shear coordinates gives a valid
/// metric satisfying the constraints with an energy. Thus, we can can plot the energy
/// for any coordinates. We plot the energies of the metrics in a two dimensional grid
/// around the initial metric

using namespace Penner;
using namespace Optimization;

int main(int argc, char *argv[])
{
  spdlog::set_level(spdlog::level::debug);
  assert(argc > 3);
  std::string input_filename = argv[1];
	std::string Th_hat_filename = argv[2];
  std::string output_dir = argv[3];
  std::string energy_choice = argv[4];
  Scalar range = std::stod(argv[5]);
  std::filesystem::create_directories(output_dir);
	int num_grid_steps = 800;

  // Get input mesh
  Eigen::MatrixXd V, uv, N;
  Eigen::MatrixXi F, FT, FN;
	spdlog::info("Plotting energy for the mesh at {}", input_filename);
  igl::readOBJ(input_filename, V, uv, N, F, FT, FN);
	
	// Get input angles
	std::vector<Scalar> Th_hat;
	spdlog::info("Using cone angles at {}", Th_hat_filename);
	read_vector_from_file(Th_hat_filename, Th_hat);

	// Get initial mesh for optimization
	std::vector<int> vtx_reindex;
	std::vector<int> free_cones = {};
	bool fix_boundary = false;
	std::unique_ptr<DifferentiableConeMetric> cone_metric = generate_initial_mesh(V, F, V, F, Th_hat, vtx_reindex, free_cones, fix_boundary, false);

	// Compute shear dual basis and the coordinates
	MatrixX shear_basis_matrix;
  std::vector<int> independent_edges;
	compute_shear_dual_basis(*cone_metric, shear_basis_matrix, independent_edges);

  // Build energy functions for given energy
  LogLengthEnergy opt_energy(*cone_metric);

  // Build independent and dependent basis vectors by adding a global scaling term
  // to the shear basis and removing and arbitrary basis vector from the scale factors
  MatrixX constraint_domain_matrix, constraint_codomain_matrix;
  VectorX domain_coords, codomain_coords;
  compute_optimization_domain(
    *cone_metric,
    shear_basis_matrix,
    constraint_domain_matrix,
    constraint_codomain_matrix,
    domain_coords,
    codomain_coords
  );
  spdlog::info(
    "Plotting {} coordinates with codomain of dimension {}",
    constraint_domain_matrix.cols(),
    constraint_codomain_matrix.cols()
  );
	Scalar x0 = domain_coords[0];
	Scalar y0 = domain_coords[1];

	// Iterate over grid
	auto proj_params = std::make_shared<ProjectionParameters>();
	Scalar delta = 2.0 * range / static_cast<Scalar>(num_grid_steps - 1);
	Eigen::MatrixXd energy_grid(num_grid_steps, num_grid_steps);
	for (int i = 0; i < num_grid_steps; ++i)
	{
		for (int j = 0; j < num_grid_steps; ++j)
		{
			// Update metric
			Scalar dx = -range + delta * i;
			Scalar dy = -range + delta * j;
			domain_coords[0] = x0 + dx;
			domain_coords[1] = y0 + dy;

			// Compute the energy for the shear metric coordinates
			Scalar energy = compute_domain_coordinate_energy(
					*cone_metric,
					opt_energy,
					constraint_domain_matrix,
					constraint_codomain_matrix,
					domain_coords,
					codomain_coords,
					proj_params);
			energy_grid(i, j) = double(energy);
		}
	}

	// Write the output
	std::string output_filename = join_path(output_dir, "energy_grid_"+energy_choice + "_range_" + argv[5]);
	write_matrix(energy_grid, output_filename);
}
