#include "ParallelEigenvectors.hh"

#include "TensorProductBezierTriangles.hh"
#include "TensorSujudiHaimesEvaluator.hh"
#include "ParallelEigenvectorsEvaluator.hh"

#include <Eigen/Eigenvalues>
#include <Eigen/LU>

#include <boost/range/algorithm/min_element.hpp>
#include <boost/range/algorithm_ext/insert.hpp>

#include <stack>
#include <queue>
#include <iterator>
#include <complex>
#include <type_traits>

namespace
{
using namespace pev;

/**
 * Representative Solution in a cluster of similar solutions
 */
template <typename Evaluator>
struct ClusterRepr
{
    std::size_t cluster_size;
    Evaluator eval;
};


/**
 * Cluster all triangles in a list that are closer than a given distance
 *
 * @param cands List of candidates generated by parallelEigenvectorSearch()
 * @param epsilon Maximum distance of triangles in a cluster
 *
 * @return List of clusters (each cluster is a list of candidates)
 */
template <typename CandList>
std::vector<CandList>
clusterTris(const CandList& cands, double epsilon)
{
    auto classes = std::vector<CandList>{};
    for(const auto& t : cands)
    {
        classes.push_back({t});
    }

    auto has_close_elements = [&](const CandList& c1,
                                  const CandList& c2) {
        if(c1 == c2) return false;
        for(const auto& t1 : c1)
        {
            for(const auto& t2 : c2)
            {
                if(distance(t1, t2) <= epsilon)
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


template <typename Evaluator>
std::vector<ClusterRepr<Evaluator>>
findRepresentatives(const std::vector<std::vector<Evaluator>>& clusters)
{
    static_assert(is_evaluator<Evaluator>::value,
                  "findRepresentatives requires a valid Evaluator!");
    auto result = std::vector<ClusterRepr<Evaluator>>{};
    for(const auto& c : clusters)
    {
        using namespace boost;
        using namespace boost::adaptors;
        result.push_back(
                {c.size(),
                 *min_element(c, [](const auto& c1, const auto& c2) {
                     return c1.error() < c2.error();
                 })});
    }
    return result;
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
template <typename Evaluator>
PointList
computeContextInfo(const std::vector<ClusterRepr<Evaluator>>& representatives,
                   const TensorInterp& s_interp,
                   const TensorInterp& t_interp,
                   const Triangle& tri)
{
    auto points = PointList{};
    points.reserve(representatives.size());

    for(const auto& r : representatives)
    {
        const auto& pos_tri = r.eval.tris().pos_tri;
        const auto& dir_tri = r.eval.tris().dir_tri;

        auto result_center = pos_tri({1. / 3., 1. / 3., 1. / 3.});
        auto result_dir = dir_tri({1. / 3., 1. / 3., 1. / 3.}).normalized();

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
                .cwiseAbs()
                .minCoeff(&s_closest_index);

        auto t_closest_index = Vec3d::Index{0};
        (t_eigvs - Vec3c::Ones() * t_real_eigv)
                .cwiseAbs()
                .minCoeff(&t_closest_index);

        // Find which of the (real) eigenvalues ours is
        auto count_larger_real = [](double ref,
                                    const std::complex<double>& val) {
            if(val.imag() != 0) return 0;
            if(std::abs(ref) >= std::abs(val.real())) return 0;
            return 1;
        };
        auto s_order = s_eigvs.unaryExpr([&](const std::complex<double>& val) {
                                  return count_larger_real(
                                          s_eigvs[s_closest_index].real(), val);
                              })
                               .sum();
        auto t_order = t_eigvs.unaryExpr([&](const std::complex<double>& val) {
                                  return count_larger_real(
                                          t_eigvs[t_closest_index].real(), val);
                              })
                               .sum();

        points.push_back(
                PEVPoint{tri(result_center),
                         ERank(s_order),
                         ERank(t_order),
                         result_dir,
                         s_real_eigv,
                         t_real_eigv,
                         s_eigvs.sum().imag() != 0,
                         t_eigvs.sum().imag() != 0,
                         r.cluster_size,
                         (pos_tri[1] - pos_tri[0]).norm(),
                         (dir_tri[1] - dir_tri[0]).norm(),
                         r.eval.condition()});
    }
    return points;
}


template <typename Evaluator>
PointList
computeContextInfoSH(const std::vector<ClusterRepr<Evaluator>>& representatives,
                     const TensorInterp& t_interp,
                     const TensorInterp& tx_interp,
                     const TensorInterp& ty_interp,
                     const TensorInterp& tz_interp,
                     const Triangle& tri)
{
    auto points = PointList{};
    points.reserve(representatives.size());

    for(const auto& r : representatives)
    {
        const auto& pos_tri = r.eval.tris().pos_tri;
        const auto& dir_tri = r.eval.tris().dir_tri;

        auto result_center = pos_tri({1. / 3., 1. / 3., 1. / 3.});
        auto result_dir = dir_tri({1. / 3., 1. / 3., 1. / 3.}).normalized();

        // We want to know which eigenvector of each tensor field we have
        // found (i.e. corresponding to largest, middle, or smallest
        // eigenvalue)
        // Therefore we explicitly compute the eigenvalues at the result
        // position and check which ones the found eigenvector direction
        // corresponds to.
        // @todo: make this step optional

        auto t = t_interp(result_center);
        auto tx = tx_interp(result_center);
        auto ty = ty_interp(result_center);
        auto tz = tz_interp(result_center);
        auto dt = (tx * result_dir[0] + ty * result_dir[1] + tz * result_dir[2])
                          .eval();

        // Get eigenvalues from our computed direction
        auto t_real_eigv = (t * result_dir).dot(result_dir);
        auto dt_real_eigv = (dt * result_dir).dot(result_dir);

        // Compute all eigenvalues using Eigen
        auto t_eigvs = t.eigenvalues().eval();
        auto dt_eigvs = dt.eigenvalues().eval();

        // Find index of eigenvalue that is closest to the one we computed
        using Vec3c = decltype(t_eigvs);
        auto t_closest_index = Vec3d::Index{0};
        (t_eigvs - Vec3c::Ones() * t_real_eigv)
                .cwiseAbs()
                .minCoeff(&t_closest_index);

        auto dt_closest_index = Vec3d::Index{0};
        (dt_eigvs - Vec3c::Ones() * dt_real_eigv)
                .cwiseAbs()
                .minCoeff(&dt_closest_index);

        // Find which of the (real) eigenvalues ours is
        auto count_larger_real = [](double ref,
                                    const std::complex<double>& val) {
            if(val.imag() != 0) return 0;
            if(std::abs(ref) >= std::abs(val.real())) return 0;
            return 1;
        };
        auto t_order = t_eigvs.unaryExpr([&](const std::complex<double>& val) {
                                  return count_larger_real(
                                          t_eigvs[t_closest_index].real(), val);
                              })
                               .sum();
        auto dt_order =
                dt_eigvs.unaryExpr([&](const std::complex<double>& val) {
                            return count_larger_real(
                                    dt_eigvs[dt_closest_index].real(), val);
                        })
                        .sum();

        points.push_back(
                PEVPoint{tri(result_center),
                         ERank(t_order),
                         ERank(dt_order),
                         result_dir,
                         t_real_eigv,
                         dt_real_eigv,
                         t_eigvs.sum().imag() != 0,
                         dt_eigvs.sum().imag() != 0,
                         r.cluster_size,
                         (pos_tri[1] - pos_tri[0]).norm(),
                         (dir_tri[1] - dir_tri[0]).norm(),
                         r.eval.condition()});
    }
    return points;
}


template <typename Evaluator,
          typename = std::enable_if_t<is_evaluator<Evaluator>::value>>
std::vector<Evaluator> rootSearch(const Evaluator& start_ev,
                                  uint64_t* num_splits = nullptr,
                                  uint64_t* max_level = nullptr)
{
    auto work_lst = std::queue<Evaluator>{};
    work_lst.push(start_ev);
    auto result = std::vector<Evaluator>{};

    while(!work_lst.empty() && work_lst.size() < std::pow(16, 3))
    {
        auto ev = work_lst.front();
        work_lst.pop();
        if(num_splits) *num_splits += 1;
        if(max_level && *max_level < ev.splitLevel())
        {
            *max_level = ev.splitLevel();
        }

        switch(ev.eval())
        {
            case Result::Split:
                for(const auto& p : ev.split())
                {
                    work_lst.push(p);
                }
                break;
            case Result::Accept:
                result.push_back(ev);
                break;
            case Result::Discard:
                break;
        }
    }

    return result;
}


std::vector<ParallelEigenvectorsEvaluator>
parallelEigenvectorSearch(const TensorInterp& s,
                          const TensorInterp& t,
                          const Triangle& tri,
                          double tolerance,
                          uint64_t* num_splits = nullptr,
                          uint64_t* max_level = nullptr)
{
    auto result = std::vector<ParallelEigenvectorsEvaluator>{};

    auto compute_tri = [&](const Triangle& r) {
        auto start_ev = ParallelEigenvectorsEvaluator(
                {tri, r}, s, t, {tolerance});
        boost::insert(result,
                      result.end(),
                      rootSearch(start_ev, num_splits, max_level));
    };

    // Four triangles covering hemisphere
    compute_tri(Triangle{{Vec3d{1, 0, 0}, Vec3d{0, 1, 0}, Vec3d{0, 0, 1}}});
    compute_tri(Triangle{{Vec3d{0, 1, 0}, Vec3d{-1, 0, 0}, Vec3d{0, 0, 1}}});
    compute_tri(Triangle{{Vec3d{-1, 0, 0}, Vec3d{0, -1, 0}, Vec3d{0, 0, 1}}});
    compute_tri(Triangle{{Vec3d{0, -1, 0}, Vec3d{1, 0, 0}, Vec3d{0, 0, 1}}});

    return result;
}


std::vector<TensorSujudiHaimesEvaluator>
tensorSujudiHaimesSearch(const TensorInterp& t,
                         const std::array<TensorInterp, 3>& dt,
                         const Triangle& tri,
                         double tolerance,
                         double min_ev,
                         uint64_t* num_splits = nullptr,
                         uint64_t* max_level = nullptr)
{
    auto result = std::vector<TensorSujudiHaimesEvaluator>{};

    auto compute_tri = [&](const Triangle& r) {
        auto start_ev = TensorSujudiHaimesEvaluator(
                {tri, r}, t, dt, {tolerance, min_ev});
        boost::insert(result,
                      result.end(),
                      rootSearch(start_ev, num_splits, max_level));
    };

    auto dir_tris = std::array<Triangle, 4>{
            Triangle{{Vec3d{1, 0, 0}, Vec3d{0, 1, 0}, Vec3d{0, 0, 1}}},
            Triangle{{Vec3d{0, 1, 0}, Vec3d{-1, 0, 0}, Vec3d{0, 0, 1}}},
            Triangle{{Vec3d{-1, 0, 0}, Vec3d{0, -1, 0}, Vec3d{0, 0, 1}}},
            Triangle{{Vec3d{0, -1, 0}, Vec3d{1, 0, 0}, Vec3d{0, 0, 1}}}};

    for(const auto& tri: dir_tris)
    {
        for(const auto& t: tri.split())
        {
            compute_tri(t);
        }
    }

    return result;
}

} // namespace


namespace pev
{

PointList findParallelEigenvectors(const std::array<Mat3d, 3>& s,
                                   const std::array<Mat3d, 3>& t,
                                   const std::array<Vec3d, 3>& x,
                                   const PEVOptions& opts)
{
    auto start_tri =
            Triangle{{Vec3d{1., 0., 0.}, Vec3d{0., 1., 0.}, Vec3d{0., 0., 1.}}};

    auto st = TensorInterp{{s[0], s[1], s[2]}};
    auto tt = TensorInterp{{t[0], t[1], t[2]}};
    auto xt = Triangle{{x[0], x[1], x[2]}};

    auto num_splits = uint64_t{0};
    auto max_level = uint64_t{0};
    auto tris = parallelEigenvectorSearch(st,
                                          tt,
                                          start_tri,
                                          opts.tolerance,
                                          &num_splits,
                                          &max_level);

    auto clustered_tris = clusterTris(tris, opts.cluster_epsilon);

    auto representatives = findRepresentatives(clustered_tris);

    return computeContextInfo(representatives, st, tt, xt);
}


PointList findParallelEigenvectors(const std::array<Mat3d, 3>& s,
                                   const std::array<Mat3d, 3>& t,
                                   const PEVOptions& opts)
{
    return findParallelEigenvectors(
            s,
            t,
            {Vec3d{1., 0., 0.}, Vec3d{0., 1., 0.}, Vec3d{0., 0., 1.}},
            opts);
}


PointList findTensorSujudiHaimes(const std::array<Mat3d, 3>& t,
                                 const std::array<std::array<Mat3d, 3>, 3>& dt,
                                 const std::array<Vec3d, 3>& x,
                                 const PEVOptions& opts)
{
    auto start_tri =
            Triangle{{Vec3d{1., 0., 0.}, Vec3d{0., 1., 0.}, Vec3d{0., 0., 1.}}};


    auto tt = TensorInterp{{t[0], t[1], t[2]}};
    auto tx = TensorInterp{{dt[0][0], dt[0][1], dt[0][2]}};
    auto ty = TensorInterp{{dt[1][0], dt[1][1], dt[1][2]}};
    auto tz = TensorInterp{{dt[2][0], dt[2][1], dt[2][2]}};
    auto xt = Triangle{{x[0], x[1], x[2]}};

    auto num_splits = uint64_t{0};
    auto max_level = uint64_t{0};
    auto tris = tensorSujudiHaimesSearch(tt,
                                         {tx, ty, tz},
                                         start_tri,
                                         opts.tolerance,
                                         opts.min_ev,
                                         &num_splits,
                                         &max_level);

    auto clustered_tris = clusterTris(tris, opts.cluster_epsilon);

    auto representatives = findRepresentatives(clustered_tris);

    return computeContextInfoSH(representatives, tt, tx, ty, tz, xt);
}


PointList findTensorSujudiHaimes(const std::array<Mat3d, 3>& t,
                                 const std::array<std::array<Mat3d, 3>, 3>& dt,
                                 const PEVOptions& opts)
{
    return findTensorSujudiHaimes(
            t,
            dt,
            {Vec3d{1., 0., 0.}, Vec3d{0., 1., 0.}, Vec3d{0., 0., 1.}},
            opts);
}

} // namespace pev
