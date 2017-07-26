#include "ParallelEigenvectors.hh"

#include "BarycentricInterpolator.hh"
#include "BezierTriangle.hh"
#include "BezierDoubleTriangle.hh"

#include <Eigen/LU>
#include <Eigen/Eigenvalues>

#include <boost/optional/optional.hpp>
#include <boost/range/join.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <iostream>
#include <algorithm>
#include <utility>
#include <array>
#include <vector>
#include <map>
#include <queue>
#include <stack>

namespace
{

using namespace pev;

/**
 * Mixed linear/quadratic polynomials on a pair of barycentric coordinates
 */
using BDoubleTri = BezierDoubleTriangle<double>;


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

    friend bool operator==(const TriPair& tx1, const TriPair& tx2)
    {
        return tx1.direction_tri == tx2.direction_tri
                && tx1.spatial_tri == tx2.spatial_tri;
    }

    friend bool operator!=(const TriPair& tx1, const TriPair& tx2)
    {
        return !(tx1 == tx2);
    }
};


/**
 * List of parallel eigenvector point candidates
 */
using TriPairList = std::vector<TriPair>;


#ifdef DRAW_DEBUG

const double red[3] = {255, 0, 0};
const double yellow[3] = {255, 127, 0};
const double green[3] = {0, 255, 0};
const double blue[3] = {128, 255, 255};
const double dark_blue[3] = {0, 0, 255};
const double white[3] = {255, 255, 255};

using Vec2d = Eigen::Vector2d;

std::array<Vec2d, 3> project_tri(const Triangle& tri, bool topdown = true)
{
    auto proj = Eigen::Matrix<double, 2, 3>{};
    static const auto sqrt23 = std::sqrt(2.0)/std::sqrt(3.0);
    if(topdown)
    {
        proj << 1, 1, 0,
                1, -1, 0;
    }
    else
    {
        proj << 1, -1, 0,
                -sqrt23, -sqrt23, sqrt23;
    }
    auto result = std::array<Vec2d, 3>{
        proj*tri.v1(),
        proj*tri.v2(),
        proj*tri.v3()
    };
    for(auto& p: result)
    {
        p = (p * pos_image.width()/2) + Vec2d(pos_image.width()/2,
                                              pos_image.height()/2);
    }
    return result;
}


void draw_tri(CImg& image,
              const Triangle& tri, const double* color,
              bool fill = true, bool topdown=true)
{
    auto projected = project_tri(tri, topdown);
    if(fill)
    {
        image.draw_triangle(int(projected[0].x()), int(projected[0].y()),
                            int(projected[1].x()), int(projected[1].y()),
                            int(projected[2].x()), int(projected[2].y()),
                            color);
    }
    else
    {
        image.draw_triangle(int(projected[0].x()), int(projected[0].y()),
                            int(projected[1].x()), int(projected[1].y()),
                            int(projected[2].x()), int(projected[2].y()),
                            color, 1.0, 0xffffffff);
    }
}


void draw_cross(CImg& image,
                const Vec3d& pos, const double* color,
                bool topdown=true)
{
    static const auto size = 10;
    auto projected = project_tri(Triangle{pos, pos, pos}, topdown);
    image.draw_line(int(projected[0].x())-size, int(projected[0].y()),
                    int(projected[0].x())+size, int(projected[0].y()),
                    color);
    image.draw_line(int(projected[0].x()), int(projected[0].y()-size),
                    int(projected[0].x()), int(projected[0].y()+size),
                    color);
}

#endif

/**
 * (Simplified) distance between two triangles
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
std::vector<TriPairList>
clusterTris(const TriPairList& tris, double epsilon)
{
    auto classes = std::vector<TriPairList>{};
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


using FindReprReturnType = std::pair<int,
                                     std::vector<std::tuple<int,
                                                            Triangle,
                                                            Triangle>>>;
/**
 * @brief Select the candidate with the most parallel eigenvectors from each
 *     cluster
 * @details Discards any points that have eigenvectors which are less parallel
 *     than @a parallelity_epsilon
 *
 * @param clusters List of candidate clusters as produced by clusterTris()
 * @param s_interp First tensor field on the triangle
 * @param t_interp Second tensor field on the triangle
 * @param parallelity_epsilon Maximum parallelity error for a candidate to be
 *     considered valid
 *
 * @return List of candidates, each a representative of a cluster
 */
FindReprReturnType
findRepresentatives(const std::vector<TriPairList>& clusters,
                                const TensorInterp& s_interp,
                                const TensorInterp& t_interp,
                                double parallelity_epsilon)
{
    auto result = std::vector<std::tuple<int, Triangle, Triangle>>{};
    auto num_fp = 0;
    for(const auto& c: clusters)
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
        // Only add representative if any candidate had sufficiently parallel
        // eigenvectors
        if(min_sin < 1.)
        {
            result.push_back(std::make_tuple(c.size(),
                                             min_angle_tri->direction_tri,
                                             min_angle_tri->spatial_tri));
        }
        else
        {
            ++num_fp;
        }
    }
    return std::make_pair(num_fp, result);
}


/**
 * @brief Compute context info for representatives
 * @details Computes global point position, eigenvalue order, presence of other
 *      imaginary eigenvalues, and packs into result list together with point
 *      position, eigenvector direction, eigenvalues.
 *
 * @param representatives TriPairs selected by findRepresentatives()
 * @param s_interp First tensor field on the triangle
 * @param t_interp Second tensor field on the triangle
 * @param tri Spatial triangle
 * @return List of PEVPoints with context info
 */
std::pair<int, PointList>
computeContextInfo(const FindReprReturnType& representatives,
                   const TensorInterp& s_interp,
                   const TensorInterp& t_interp,
                   const Triangle& tri)
{
    auto points = PointList{};
    points.reserve(representatives.second.size());

    for(const auto& r: representatives.second)
    {
        auto result_center = std::get<2>(r)(1./3., 1./3., 1./3.);
        auto result_dir = std::get<1>(r)(1./3., 1./3., 1./3.).normalized();

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
        using Vec3c = decltype(s_eigvs);
        auto s_closest_index = Vec3d::Index{0};
        (s_eigvs - Vec3c::Ones() * s_real_eigv)
                .cwiseAbs().minCoeff(&s_closest_index);

        auto t_closest_index = Vec3d::Index{0};
        (t_eigvs - Vec3c::Ones() * t_real_eigv)
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

        points.push_back(PEVPoint{tri(result_center),
                          ERank(s_order),
                          ERank(t_order),
                          result_dir,
                          s_real_eigv,
                          t_real_eigv,
                          s_eigvs.sum().imag() != 0,
                          t_eigvs.sum().imag() != 0,
                          std::get<0>(r)});
    }
    return std::make_pair(representatives.first, points);
}


/**
 * Check if all coefficients are positive or negative
 *
 * @return 1 for all positive, -1 for all negative, 0 otherwise
 */
int sameSign(const BDoubleTri& coeffs)
{
    auto ma = coeffs.coefficients().maxCoeff();
    auto mi = coeffs.coefficients().minCoeff();
    return mi*ma > 0 ? sgn(ma) : 0;
}


std::array<BDoubleTri, 3>
bezierDoubleCoeffs(const Mat3d& t1, const Mat3d& t2, const Mat3d& t3,
                   const Vec3d& r1, const Vec3d& r2, const Vec3d& r3)
{
    auto coeffs = Eigen::Matrix<double, 18, 3>{};

    coeffs.row(BDoubleTri::i100200) = (t1*r1).cross(r1);
    coeffs.row(BDoubleTri::i100020) = (t1*r2).cross(r2);
    coeffs.row(BDoubleTri::i100002) = (t1*r3).cross(r3);

    coeffs.row(BDoubleTri::i010200) = (t2*r1).cross(r1);
    coeffs.row(BDoubleTri::i010020) = (t2*r2).cross(r2);
    coeffs.row(BDoubleTri::i010002) = (t2*r3).cross(r3);

    coeffs.row(BDoubleTri::i001200) = (t3*r1).cross(r1);
    coeffs.row(BDoubleTri::i001020) = (t3*r2).cross(r2);
    coeffs.row(BDoubleTri::i001002) = (t3*r3).cross(r3);

    coeffs.row(BDoubleTri::i100110) = ((t1*r1).cross(r2) + (t1*r2).cross(r1))/2;
    coeffs.row(BDoubleTri::i100011) = ((t1*r2).cross(r3) + (t1*r3).cross(r2))/2;
    coeffs.row(BDoubleTri::i100101) = ((t1*r3).cross(r1) + (t1*r1).cross(r3))/2;

    coeffs.row(BDoubleTri::i010110) = ((t2*r1).cross(r2) + (t2*r2).cross(r1))/2;
    coeffs.row(BDoubleTri::i010011) = ((t2*r2).cross(r3) + (t2*r3).cross(r2))/2;
    coeffs.row(BDoubleTri::i010101) = ((t2*r3).cross(r1) + (t2*r1).cross(r3))/2;

    coeffs.row(BDoubleTri::i001110) = ((t3*r1).cross(r2) + (t3*r2).cross(r1))/2;
    coeffs.row(BDoubleTri::i001011) = ((t3*r2).cross(r3) + (t3*r3).cross(r2))/2;
    coeffs.row(BDoubleTri::i001101) = ((t3*r3).cross(r1) + (t3*r1).cross(r3))/2;

    return {BDoubleTri(coeffs.col(0)),
            BDoubleTri(coeffs.col(1)),
            BDoubleTri(coeffs.col(2))};
}


TriPairList parallelEigenvectorSearchNew(const TensorInterp& s,
                                         const TensorInterp& t,
                                         const Triangle& tri,
                                         double spatial_epsilon,
                                         double direction_epsilon,
                                         uint64_t* num_splits = nullptr)
{
    // Structure for holding information needed during subdivision
    struct SubPackage
    {
        TriPair trip;
        TensorInterp s;
        TensorInterp t;
        std::array<BDoubleTri, 3> s_funcs;
        std::array<BDoubleTri, 3> t_funcs;
        bool last_split_dir;
    };

    #ifdef DRAW_DEBUG
    pos_image.fill(0);
    dir_image.fill(0);
    #endif

    auto tstck = std::stack<SubPackage>{};

    auto init_tri = [&](const Triangle& r)
    {
        auto s_coeffs = bezierDoubleCoeffs(s.v1(), s.v2(), s.v3(),
                                           r.v1(), r.v2(), r.v3());
        auto t_coeffs = bezierDoubleCoeffs(t.v1(), t.v2(), t.v3(),
                                           r.v1(), r.v2(), r.v3());
        tstck.push(SubPackage{{r, tri}, s, t, s_coeffs, t_coeffs, true});
    };

    // Start with four triangles covering hemisphere
    init_tri(Triangle{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}});
    init_tri(Triangle{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}});
    init_tri(Triangle{{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}});
    init_tri(Triangle{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}});

    auto result = TriPairList{};

    while(!tstck.empty())
    {
        auto pack = tstck.top();
        tstck.pop();
        if(num_splits) *num_splits += 1;

        // Check if any of the error components can not become zero in the
        // current subdivision triangles
        auto has_nonzero = boost::algorithm::any_of(
                boost::join(pack.s_funcs, pack.t_funcs),
                [](const BDoubleTri& c){ return sameSign(c) != 0; });

        // Discard triangles if no roots can occur inside
        if(has_nonzero)
        {
            #ifdef DRAW_DEBUG
            draw_tri(pos_image, pack.trip.spatial_tri, red, false, false);
            draw_tri(dir_image, pack.trip.direction_tri, red, false, true);
            pos_frame.display(pos_image);
            dir_frame.display(dir_image);
            #endif
            continue;
        }

        // If maximum subdivision accuracy reached, accept point as solution
        auto dir_sub_reached = (pack.trip.direction_tri.v1()
                                - pack.trip.direction_tri.v2()).norm()
                               < direction_epsilon;
        auto pos_sub_reached = (pack.trip.spatial_tri.v1()
                                - pack.trip.spatial_tri.v2()).norm()
                               < spatial_epsilon;

        if(pos_sub_reached && dir_sub_reached)
        {
            result.push_back(pack.trip);
            #ifdef DRAW_DEBUG
            draw_tri(pos_image, pack.trip.spatial_tri, green, false, false);
            draw_tri(dir_image, pack.trip.direction_tri, green, false, true);
            pos_frame.display(pos_image);
            dir_frame.display(dir_image);
            #endif
            continue;
        }

        // Subdivide
        if(pack.last_split_dir && !pos_sub_reached)
        {
            auto spatial_tri_subs = pack.trip.spatial_tri.split();
            auto s_subs = pack.s.split();
            auto t_subs = pack.t.split();
            auto s_funcs_subs = std::array<std::array<BDoubleTri, 4>, 3>{};
            auto t_funcs_subs = std::array<std::array<BDoubleTri, 4>, 3>{};
            for(auto i: range(3))
            {
                s_funcs_subs[i] = pack.s_funcs[i].split_pos();
                t_funcs_subs[i] = pack.t_funcs[i].split_pos();
            }

            for(auto i: range(4))
            {
                tstck.push(SubPackage{
                        {pack.trip.direction_tri, spatial_tri_subs[i]},
                        s_subs[i],
                        t_subs[i],
                        {s_funcs_subs[0][i],
                         s_funcs_subs[1][i],
                         s_funcs_subs[2][i]},
                        {t_funcs_subs[0][i],
                         t_funcs_subs[1][i],
                         t_funcs_subs[2][i]},
                        false});
            }
        }
        else
        {
            auto dir_tri_subs = pack.trip.direction_tri.split();
            auto s_funcs_subs = std::array<std::array<BDoubleTri, 4>, 3>{};
            auto t_funcs_subs = std::array<std::array<BDoubleTri, 4>, 3>{};
            for(auto i: range(3))
            {
                s_funcs_subs[i] = pack.s_funcs[i].split_dir();
                t_funcs_subs[i] = pack.t_funcs[i].split_dir();
            }

            for(auto i: range(4))
            {
                tstck.push(SubPackage{
                        {dir_tri_subs[i], pack.trip.spatial_tri},
                        pack.s,
                        pack.t,
                        {s_funcs_subs[0][i],
                         s_funcs_subs[1][i],
                         s_funcs_subs[2][i]},
                        {t_funcs_subs[0][i],
                         t_funcs_subs[1][i],
                         t_funcs_subs[2][i]},
                        true});
            }
        }

        #ifdef DRAW_DEBUG
        draw_tri(pos_image, pack.trip.spatial_tri, yellow, false, false);
        draw_tri(dir_image, pack.trip.direction_tri, yellow, false, true);
        pos_frame.display(pos_image);
        dir_frame.display(dir_image);
        #endif
    }

    return result;
}

} // namespace

namespace pev
{

#ifdef DRAW_DEBUG
CImg pos_image(1024, 1024, 1, 3);
CImg dir_image(1024, 1024, 1, 3);
CImgDisplay pos_frame;
CImgDisplay dir_frame;
#endif

std::pair<int, PointList> findParallelEigenvectors(
        const Mat3d& s1, const Mat3d& s2, const Mat3d& s3,
        const Mat3d& t1, const Mat3d& t2, const Mat3d& t3,
        const Vec3d& x1, const Vec3d& x2, const Vec3d& x3,
        double spatial_epsilon, double direction_epsilon,
        double cluster_epsilon, double parallelity_epsilon)
{
    auto tri = Triangle{x1, x2, x3};

    auto start_tri = Triangle{Vec3d{1., 0., 0.},
                              Vec3d{0., 1., 0.},
                              Vec3d{0., 0., 1.}};

    auto s_interp = TensorInterp{s1, s2, s3};
    auto t_interp = TensorInterp{t1, t2, t3};

    auto num_splits = uint64_t{0};
    auto tris = parallelEigenvectorSearchNew(s_interp, t_interp, start_tri,
                                             spatial_epsilon, direction_epsilon,
                                             &num_splits);

    auto clustered_tris = clusterTris(tris, cluster_epsilon);

    auto representatives = findRepresentatives(clustered_tris,
                                               s_interp, t_interp,
                                               parallelity_epsilon);


    std::cerr << representatives.second.size() << "\t" << num_splits << std::endl;

    return computeContextInfo(representatives, s_interp, t_interp, tri);
}


std::pair<int, PointList> findParallelEigenvectors(
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

} // namespace pev
