#include "ParallelEigenvectors.hh"

#include "BarycentricInterpolator.hh"
#include "BezierTriangle.hh"

#include <Eigen/LU>
#include <Eigen/Eigenvalues>

#include <boost/optional/optional.hpp>

#include <iostream>
#include <algorithm>
#include <utility>
#include <array>
#include <map>
#include <queue>
#include <stack>
#include <chrono>

namespace
{

using namespace peigv;

/**
 * Struct for computing coefficients of a cubic Bézier triangle
 */
// struct BCoeffs
// {
//     double operator[](int i) const
//     {
//         switch(i)
//         {
//             case i300:
//             {
//                 return A.determinant();
//             }
//             case i030:
//             {
//                 return B.determinant();
//             }
//             case i003:
//             {
//                 return C.determinant();

//             }
//             case i210:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << B.col(0), A.col(1), A.col(2)).finished().determinant()
//                     + (Mat3d{} << A.col(0), B.col(1), A.col(2)).finished().determinant()
//                     + (Mat3d{} << A.col(0), A.col(1), B.col(2)).finished().determinant()
//                     );

//             }
//             case i201:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << C.col(0), A.col(1), A.col(2)).finished().determinant()
//                     + (Mat3d{} << A.col(0), C.col(1), A.col(2)).finished().determinant()
//                     + (Mat3d{} << A.col(0), A.col(1), C.col(2)).finished().determinant()
//                     );

//             }
//             case i120:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << A.col(0), B.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), A.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), B.col(1), A.col(2)).finished().determinant()
//                     );

//             }
//             case i021:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << C.col(0), B.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), C.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), B.col(1), C.col(2)).finished().determinant()
//                     );

//             }
//             case i102:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << A.col(0), C.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), A.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), C.col(1), A.col(2)).finished().determinant()
//                     );

//             }
//             case i012:
//             {
//                 return 1. / 3. * (
//                       (Mat3d{} << B.col(0), C.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), B.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), C.col(1), B.col(2)).finished().determinant()
//                     );

//             }
//             case i111:
//             {
//                 return 1. / 6. * (
//                       (Mat3d{} << A.col(0), B.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << A.col(0), C.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), A.col(1), C.col(2)).finished().determinant()
//                     + (Mat3d{} << B.col(0), C.col(1), A.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), A.col(1), B.col(2)).finished().determinant()
//                     + (Mat3d{} << C.col(0), B.col(1), A.col(2)).finished().determinant()
//                     );
//             }
//             default:
//                 return 0.;
//         }
//     }

//     static constexpr std::size_t size()
//     {
//         return 10;
//     }

//     Mat3d A;
//     Mat3d B;
//     Mat3d C;
// };


/**
 * Cubic polynomial of degree 3 in barycentric coordinates
 */
using BezierTriangle = BezierTriangle<double>;


/**
 * Linear tensor field expressed in barycentric coordinates
 */
using TensorInterp = BarycetricInterpolator<Mat3d>;


/**
 * Triangle in 3d expressed in barycentric coordinates
 */
using Triangle = BarycetricInterpolator<Vec3d>;


/**
 * Aggregate representing a candidate solution for a parallel eigenvector point.
 *
 * Holds the triangle in direction space that contains the potential
 * eigenvector, and the triangle in barycentric coordinate space that contains
 * the potential eigenvector point.
 */
struct TriPair
{
    Triangle direction_tri;
    Triangle spatial_tri;

    friend bool operator==(const TriPair& tp1, const TriPair& tp2)
    {
        return tp1.direction_tri == tp2.direction_tri
                && tp1.spatial_tri == tp2.spatial_tri;
    }

    friend bool operator!=(const TriPair& tp1, const TriPair& tp2)
    {
        return !(tp1 == tp2);
    }
};


/**
 * List of parallel eigenvector point candidates
 */
using TriPairList = std::list<TriPair>;

/**
 * @brief (simplified) distance between two triangles
 * @details Computes smallest distance between two triangle corners
 */
double distance(const Triangle& t1, const Triangle& t2)
{
    return (t1(1./3., 1./3., 1./3.) - t2(1./3., 1./3., 1./3.)).norm();
}


/**
 * Cluster all triangles in a list that are closer than a given distance
 *
 * @param tris List of candidates generated by parallelEigenvectorSearch()
 * @param epsilon Maximum distance of triangles in a cluster
 *
 * @return List of clusters (each cluster is a list of candidates)
 */
std::list<TriPairList>
clusterTris(const TriPairList& tris, double epsilon)
{
    auto classes = std::list<TriPairList>{};
    for(const auto& t: tris)
    {
        classes.push_back({t});
    }

    auto has_close_elements = [&](const TriPairList& c1, const TriPairList& c2)
    {
        if(c1 == c2) return false;
        for(const auto& t1: c1)
        {
            for(const auto& t2: c2)
            {
                if(distance(t1.spatial_tri, t2.spatial_tri) <= epsilon)
                {
                    return true;
                }
            }
        }
        return false;
    };

    auto changed = true;
    while(changed)
    {
        changed = false;
        for(auto it = std::begin(classes); it != std::end(classes); ++it)
        {
            auto jt = it;
            ++jt;
            for(; jt != std::end(classes); ++jt)
            {
                if(has_close_elements(*it, *jt))
                {
                    it->insert(std::end(*it), std::begin(*jt), std::end(*jt));
                    jt = classes.erase(jt);
                    --jt;
                    changed = true;
                }
            }
        }
    }
    return classes;
}


/**
 * Check if all coefficients are positive or negative
 *
 * @return 1 for all positive, -1 for all negative, 0 otherwise
 */
int sameSign(const BezierTriangle& coeffs)
{
    auto ma = coeffs.coefficients().maxCoeff();
    auto mi = coeffs.coefficients().minCoeff();
    return mi*ma > 0 ? sgn(ma) : 0;
}


BezierTriangle computeBezierTriangle(const Mat3d& A,
                                     const Mat3d& B,
                                     const Mat3d& C)
{
    auto result = BezierTriangle::Coeffs{};

    result(BezierTriangle::i300) = A.determinant();
    result(BezierTriangle::i030) = B.determinant();
    result(BezierTriangle::i003) = C.determinant();
    result(BezierTriangle::i210) = 1. / 3. * (
              (Mat3d{} << B.col(0), A.col(1), A.col(2)).finished().determinant()
            + (Mat3d{} << A.col(0), B.col(1), A.col(2)).finished().determinant()
            + (Mat3d{} << A.col(0), A.col(1), B.col(2)).finished().determinant()
            );
    result(BezierTriangle::i201) = 1. / 3. * (
              (Mat3d{} << C.col(0), A.col(1), A.col(2)).finished().determinant()
            + (Mat3d{} << A.col(0), C.col(1), A.col(2)).finished().determinant()
            + (Mat3d{} << A.col(0), A.col(1), C.col(2)).finished().determinant()
            );
    result(BezierTriangle::i120) = 1. / 3. * (
              (Mat3d{} << A.col(0), B.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), A.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), B.col(1), A.col(2)).finished().determinant()
            );
    result(BezierTriangle::i021) = 1. / 3. * (
              (Mat3d{} << C.col(0), B.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), C.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), B.col(1), C.col(2)).finished().determinant()
            );
    result(BezierTriangle::i102) = 1. / 3. * (
              (Mat3d{} << A.col(0), C.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), A.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), C.col(1), A.col(2)).finished().determinant()
            );
    result(BezierTriangle::i012) = 1. / 3. * (
              (Mat3d{} << B.col(0), C.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), B.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), C.col(1), B.col(2)).finished().determinant()
            );
    result(BezierTriangle::i111) = 1. / 6. * (
              (Mat3d{} << A.col(0), B.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << A.col(0), C.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), A.col(1), C.col(2)).finished().determinant()
            + (Mat3d{} << B.col(0), C.col(1), A.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), A.col(1), B.col(2)).finished().determinant()
            + (Mat3d{} << C.col(0), B.col(1), A.col(2)).finished().determinant()
            );
    return BezierTriangle{result};
}


std::array<BezierTriangle, 3>
bezierCoefficients(const Mat3d& t1, const Mat3d& t2, const Mat3d& t3,
                   const Vec3d& r1, const Vec3d& r2, const Vec3d& r3)
{
    auto A = Mat3d{};
    A << t1 * r1, t2 * r1, t3 * r1;

    auto B = Mat3d{};
    B << t1 * r2, t2 * r2, t3 * r2;

    auto C = Mat3d{};
    C << t1 * r3, t2 * r3, t3 * r3;

    auto alpha_coeffs = computeBezierTriangle(
            (Mat3d{} << r1, A.col(1), A.col(2)).finished(),
            (Mat3d{} << r2, B.col(1), B.col(2)).finished(),
            (Mat3d{} << r3, C.col(1), C.col(2)).finished());

    auto beta_coeffs = computeBezierTriangle(
            (Mat3d{} << A.col(0), r1, A.col(2)).finished(),
            (Mat3d{} << B.col(0), r2, B.col(2)).finished(),
            (Mat3d{} << C.col(0), r3, C.col(2)).finished());

    auto gamma_coeffs = computeBezierTriangle(
            (Mat3d{} << A.col(0), A.col(1), r1).finished(),
            (Mat3d{} << B.col(0), B.col(1), r2).finished(),
            (Mat3d{} << C.col(0), C.col(1), r3).finished());

    // auto denom_coeffs = computeBezierTriangle(A, B, C);

    return {alpha_coeffs, beta_coeffs, gamma_coeffs};
}

/**
 * Find a direction that might become an eigenvector somewhere inside the
 * barycentric triangle for both @a s and @a t.
 *
 * @param s First tensor field
 * @param t Second tensor field
 * @param epsilon minimum cell size when subdividing
 * @param num_subdivisions place to store the number of triangle splits
 *        perfomed during the subdivision
 * @return Triangle containing a potential eigenvector direction, or boost::none
 */
boost::optional<Triangle> findEigenDir(const TensorInterp& s,
                                       const TensorInterp& t,
                                       double epsilon)
{
    struct SubPackage
    {
        Triangle tri;
        BezierTriangle s_alpha;
        BezierTriangle s_beta;
        BezierTriangle s_gamma;
        BezierTriangle t_alpha;
        BezierTriangle t_beta;
        BezierTriangle t_gamma;

        std::array<SubPackage, 4> split() const
        {
            auto tri_sub = tri.split();
            auto s_alpha_sub = s_alpha.split();
            auto s_beta_sub = s_beta.split();
            auto s_gamma_sub = s_gamma.split();
            auto t_alpha_sub = t_alpha.split();
            auto t_beta_sub = t_beta.split();
            auto t_gamma_sub = t_gamma.split();
            return {
                SubPackage{tri_sub[0],
                    s_alpha_sub[0], s_beta_sub[0], s_gamma_sub[0],
                    t_alpha_sub[0], t_beta_sub[0], t_gamma_sub[0]},
                SubPackage{tri_sub[1],
                    s_alpha_sub[1], s_beta_sub[1], s_gamma_sub[1],
                    t_alpha_sub[1], t_beta_sub[1], t_gamma_sub[1]},
                SubPackage{tri_sub[2],
                    s_alpha_sub[2], s_beta_sub[2], s_gamma_sub[2],
                    t_alpha_sub[2], t_beta_sub[2], t_gamma_sub[2]},
                SubPackage{tri_sub[3],
                    s_alpha_sub[3], s_beta_sub[3], s_gamma_sub[3],
                    t_alpha_sub[3], t_beta_sub[3], t_gamma_sub[3]}
            };
        }
    };

    // Stack for direction triangles that still have to be processed
    // results in a depth-first search
    auto tstck = std::stack<SubPackage>{};

    auto init_tri = [&](const Triangle& tri)
    {
        auto s_coeffs = bezierCoefficients(s.v1(), s.v2(), s.v3(),
                                           tri.v1(), tri.v2(), tri.v3());
        auto t_coeffs = bezierCoefficients(t.v1(), t.v2(), t.v3(),
                                           tri.v1(), tri.v2(), tri.v3());
        tstck.push(SubPackage{
            tri,
            s_coeffs[0], s_coeffs[1], s_coeffs[2],
            t_coeffs[0], t_coeffs[1], t_coeffs[2]
        });
    };

    // Start with four triangles covering all directions in a hemisphere
    init_tri(Triangle{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}});
    init_tri(Triangle{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}});
    init_tri(Triangle{{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}});
    init_tri(Triangle{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}});

    while(!tstck.empty())
    {
        auto pack = tstck.top();
        tstck.pop();

        auto s_signs = std::array<int, 3>{sameSign(pack.s_alpha),
                                          sameSign(pack.s_beta),
                                          sameSign(pack.s_gamma)};
        auto t_signs = std::array<int, 3>{sameSign(pack.t_alpha),
                                          sameSign(pack.t_beta),
                                          sameSign(pack.t_gamma)};

        // Bezier coefficients have different signs: parallel eigenvectors
        // not possible
        auto s_minmax = std::minmax_element(std::begin(s_signs),
                                            std::end(s_signs));
        auto t_minmax = std::minmax_element(std::begin(t_signs),
                                            std::end(t_signs));
        if((*s_minmax.first) * (*s_minmax.second) < 0
           || (*t_minmax.first) * (*t_minmax.second) < 0)
        {
            continue;
        }

        // All same signs: parallel eigenvectors possible
        if(std::abs(std::accumulate(std::begin(s_signs),
                                    std::end(s_signs), 0)) == 3
           && std::abs(std::accumulate(std::begin(t_signs),
                                       std::end(t_signs), 0)) == 3)
        {
            return pack.tri;
        }

        // Small triangle and still not sure if parallel eigenvectors
        // impossible: accept direction as candidate
        if((pack.tri.v1() - pack.tri.v2()).norm() < epsilon)
        {
            return pack.tri;
        }

        // Subdivide triangle
        for(const auto& p: pack.split())
        {
            tstck.push(p);
        }
    }
    return boost::none;
}


TriPairList parallelEigenvectorSearch(const TensorInterp& s,
                                      const TensorInterp& t,
                                      const Triangle& tri,
                                      double spatial_epsilon,
                                      double direction_epsilon)
{
    struct SubPackage
    {
        TensorInterp s;
        TensorInterp t;
        Triangle tri;
    };

    // Queue for sub-triangles that still have to be processed
    // results in a breadth-first search
    auto pque = std::queue<SubPackage>{};
    pque.push({s, t, tri});

    auto result = TriPairList{};

    while(!pque.empty())
    {
        auto pack = pque.front();
        pque.pop();
        if(pque.size() > 10000)
        {
            std::cout << "Aborting eigenvector point search. "
                      << "Too many subdivisions." << std::endl;
            return result;
        }
        auto dir = findEigenDir(pack.s, pack.t, direction_epsilon);

        // No possible eigenvector directions found: discard triangle
        if(!dir) continue;

        // Triangle at subdivision limit and still viable eigenvector directions:
        // accept as parallel eigenvector point candidate
        if((pack.tri.v1() - pack.tri.v2()).norm() < spatial_epsilon)
        {
            result.push_back({dir.value(), pack.tri});
            continue;
        }

        // Subdivide triangle
        auto s_subs = pack.s.split();
        auto t_subs = pack.t.split();
        auto tri_subs = pack.tri.split();

        for(auto i: range(s_subs.size()))
        {
            pque.push({s_subs[i], t_subs[i], tri_subs[i]});
        }
    }
    return result;
}

} // namespace

namespace peigv
{

PointList findParallelEigenvectors(
        const Mat3d& s1, const Mat3d& s2, const Mat3d& s3,
        const Mat3d& t1, const Mat3d& t2, const Mat3d& t3,
        const Vec3d& p1, const Vec3d& p2, const Vec3d& p3,
        double spatial_epsilon, double direction_epsilon,
        double cluster_epsilon, double parallelity_epsilon)
{
    using namespace std::chrono;
    using milliseconds = duration<double, milliseconds::period>;

    static auto total_times = milliseconds(0.);
    static auto avg_time = total_times;
    static auto num_calls = 0;

    auto start = high_resolution_clock::now();

    auto tri = Triangle{p1, p2, p3};

    auto start_tri = Triangle{Vec3d{1., 0., 0.},
                              Vec3d{0., 1., 0.},
                              Vec3d{0., 0., 1.}};

    auto s_interp = TensorInterp{s1, s2, s3};
    auto t_interp = TensorInterp{t1, t2, t3};

    auto tris = parallelEigenvectorSearch(
            s_interp, t_interp,
            start_tri, spatial_epsilon, direction_epsilon);

    auto clustered_tris = clusterTris(tris, cluster_epsilon);

    auto points = PointList{};

    for(const auto& c: clustered_tris)
    {
        const auto* min_angle_tri = &*(c.cbegin());
        auto min_sin = 1.;
        for(const auto& trip: c)
        {
            auto dir = trip.direction_tri(1./3., 1./3., 1./3.).normalized();
            auto center = trip.spatial_tri(1./3., 1./3., 1./3.);
            auto s = s_interp(center);
            auto t = t_interp(center);

            // compute error as sum of deviations from input direction
            // after multiplication with tensors
            auto ms = (s * dir).normalized().cross(dir.normalized()).norm()
                      + (t * dir).normalized().cross(dir.normalized()).norm();

            // check if the error measure is low enough to consider the point a
            // parallel eigenvector point
            if(ms > parallelity_epsilon)
            {
                continue;
            }

            if(ms < min_sin)
            {
                min_sin = ms;
                min_angle_tri = &trip;
            }
        }
        if(min_sin < 1.)
        {
            auto result_center =
                    min_angle_tri->spatial_tri(1./3., 1./3., 1./3.);
            auto result_dir =
                    min_angle_tri->direction_tri(1./3., 1./3., 1./3.)
                    .normalized();

            // We want to know which eigenvector of each tensor field we have
            // found (i.e. corresponding to largest, middle, or smallest
            // eigenvalue)
            // Therefore we explicitly compute the eigenvalues at the result
            // position and check which ones the found eigenvector direction
            // corresponds to.
            // @todo: make this step optional

            auto s = s_interp(result_center);
            auto t = t_interp(result_center);

            // Get eigenvalues from our computed direction
            auto s_real_eigv = (s * result_dir).dot(result_dir);
            auto t_real_eigv = (t * result_dir).dot(result_dir);

            // Compute all eigenvalues using Eigen
            auto s_eigvs = s.eigenvalues().eval();
            auto t_eigvs = t.eigenvalues().eval();

            // Find index of eigenvalue that is closest to the one we computed
            using vec3c = decltype(s_eigvs);
            auto s_closest_index = Vec3d::Index{0};
            (s_eigvs - vec3c::Ones() * s_real_eigv)
                    .cwiseAbs().minCoeff(&s_closest_index);

            auto t_closest_index = Vec3d::Index{0};
            (t_eigvs - vec3c::Ones() * t_real_eigv)
                    .cwiseAbs().minCoeff(&t_closest_index);

            // Find which of the (real) eigenvalues ours is
            auto count_larger_real = [](double ref,
                                         const std::complex<double>& val)
            {
                if(val.imag() != 0) return 0;
                if(std::abs(ref) >= std::abs(val.real())) return 0;
                return 1;
            };
            auto s_order = s_eigvs.unaryExpr(
                    [&](const std::complex<double>& val)
                    {
                        return count_larger_real(
                                s_eigvs[s_closest_index].real(), val);
                    }).sum();
            auto t_order = t_eigvs.unaryExpr(
                    [&](const std::complex<double>& val)
                    {
                        return count_larger_real(
                                t_eigvs[t_closest_index].real(), val);
                    }).sum();

            // std::cout << "Final Eigenvector point: "
            //           << print(result_center) << std::endl;
            // std::cout << "Final Eigenvector direction: "
            //           << print(result_dir) << std::endl;
            // std::cout << "Final S Eigenvalue: "
            //           << s_real_eigv << std::endl;
            // std::cout << "Final T Eigenvalue: "
            //           << t_real_eigv << std::endl;
            // std::cout << "Final S Matrix: \n" << s << std::endl;
            // std::cout << "Final T Matrix: \n" << t << std::endl;
            // auto errvec = Eigen::Matrix<double, 6, 1>{};
            // errvec.topRows<3>() = (s*result_dir).cross(result_dir);
            // errvec.bottomRows<3>() = (t*result_dir).cross(result_dir);
            // std::cout << "Error vector: \n" << errvec << std::endl;

            points.push_back({tri(result_center),
                              ERank(s_order),
                              ERank(t_order),
                              result_dir,
                              s_real_eigv,
                              t_real_eigv,
                              s_eigvs.sum().imag() != 0,
                              t_eigvs.sum().imag() != 0});
        }
    }
    auto end = high_resolution_clock::now();
    auto duration = milliseconds(end - start);
    total_times += duration;
    ++num_calls;
    avg_time = total_times / num_calls;

    return points;
}


PointList findParallelEigenvectors(
        const Mat3d& s1, const Mat3d& s2, const Mat3d& s3,
        const Mat3d& t1, const Mat3d& t2, const Mat3d& t3,
        double spatial_epsilon, double direction_epsilon,
        double cluster_epsilon, double parallelity_epsilon)
{
    return findParallelEigenvectors(
            s1, s2, s3, t1, t2, t3,
            Vec3d{1, 0, 0}, Vec3d{0, 1, 0}, Vec3d{0, 0, 1},
            spatial_epsilon, direction_epsilon,
            cluster_epsilon, parallelity_epsilon);
}

} // namespace peigv