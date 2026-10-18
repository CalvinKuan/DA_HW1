// blockage.cpp -- requirements (1), (4) and (5): build the big blockage,
// carve out the pins, and slice the result into rectangles.
#include "bpv.h"

#include <boost/geometry/strategies/buffer.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

namespace bpv {
namespace {

constexpr double kEps = 1e-9;

// Grow a shape outward by `spacing`.
//
// Corners use a miter (square) join rather than round. A strict DRC spacing rule
// measures the shortest distance between two shapes, which rounds the corners, but
// layout and LEF conventionally expand rectangles squarely -- and round corners
// would not slice into clean rectangles anyway.
MultiPolygon grow(const Polygon& p, double spacing) {
    namespace buf = bg::strategy::buffer;

    const buf::distance_symmetric<double> dist(spacing);
    const buf::side_straight side;
    const buf::join_miter    join;
    const buf::end_flat      end;
    const buf::point_square  point;

    MultiPolygon out;
    bg::buffer(p, out, dist, side, join, end, point);
    return out;
}

// Shorthands for the box corners, used heavily below
inline double bx0(const Box& b) { return bg::get<bg::min_corner, 0>(b); }
inline double by0(const Box& b) { return bg::get<bg::min_corner, 1>(b); }
inline double bx1(const Box& b) { return bg::get<bg::max_corner, 0>(b); }
inline double by1(const Box& b) { return bg::get<bg::max_corner, 1>(b); }

// Merge slices that sit directly on top of one another and span the same x range.
//
// Why this is needed: Boost's buffer leaves collinear vertices where the original
// corners were. Pin B at (38,10)-(40,13), for instance, still has vertices at y=10
// and y=13 after being grown, even though those points only sit partway along a
// vertical edge. The scan line treats every such y as a cut, producing fragments
// that are correct in area but unnecessary (11 pieces instead of 7 for this case).
// Merging after the fact is sturdier than trying to detect which vertices are
// collinear beforehand.
std::vector<Box> mergeStacked(std::vector<Box> in) {
    std::sort(in.begin(), in.end(), [](const Box& a, const Box& b) {
        if (bx0(a) != bx0(b)) return bx0(a) < bx0(b);
        if (bx1(a) != bx1(b)) return bx1(a) < bx1(b);
        return by0(a) < by0(b);
    });

    std::vector<Box> out;
    for (const Box& r : in) {
        if (!out.empty()) {
            const Box& last = out.back();
            const bool sameCol  = std::abs(bx0(last) - bx0(r)) < kEps &&
                                  std::abs(bx1(last) - bx1(r)) < kEps;
            const bool touching = std::abs(by1(last) - by0(r)) < kEps;
            if (sameCol && touching) {
                out.back() = Box(Point(bx0(last), by0(last)), Point(bx1(r), by1(r)));
                continue;
            }
        }
        out.push_back(r);
    }

    // Bottom to top, left to right, so the printed listing reads naturally
    std::sort(out.begin(), out.end(), [](const Box& a, const Box& b) {
        if (by0(a) != by0(b)) return by0(a) < by0(b);
        return bx0(a) < bx0(b);
    });
    return out;
}

// Horizontal scan-line slicing: cut at the y of every vertex. Within a band the
// cross-section cannot change, so each piece that falls out is a rectangle.
//
// This is exact for rectilinear (Manhattan) geometry, which covers the assignment's
// test case and the vast majority of real M1. If the input contains diagonal edges a
// band yields a trapezoid instead; the code then falls back to that piece's bounding
// box and warns. Erring large is the safe direction for a blockage -- a blockage that
// is too big only makes the router more conservative, while one that is too small can
// let the router run into existing metal.
std::vector<Box> sliceToRects(const MultiPolygon& region) {
    std::vector<Box> out;
    if (bg::is_empty(region)) return out;

    Box bb;
    bg::envelope(region, bb);
    const double x0 = bg::get<bg::min_corner, 0>(bb);
    const double x1 = bg::get<bg::max_corner, 0>(bb);

    // Collect the y of every vertex to use as cut lines
    std::set<double> cuts;
    for (const Polygon& p : region)
        bg::for_each_point(p, [&cuts](const Point& pt) { cuts.insert(bg::get<1>(pt)); });

    const std::vector<double> ys(cuts.begin(), cuts.end());
    int notRect = 0;

    for (std::size_t i = 0; i + 1 < ys.size(); ++i) {
        const double yb = ys[i], yt = ys[i + 1];
        if (yt - yb < kEps) continue;

        // Widen the band slightly so it certainly spans the whole row
        const Box band(Point(x0 - 1.0, yb), Point(x1 + 1.0, yt));

        MultiPolygon pieces;
        bg::intersection(region, band, pieces);

        for (const Polygon& piece : pieces) {
            const double a = bg::area(piece);
            if (a < kEps) continue;

            Box e;
            bg::envelope(piece, e);
            if (std::abs(a - bg::area(e)) > kEps * std::max(1.0, a)) ++notRect;
            out.push_back(e);
        }
    }

    if (notRect > 0) {
        std::cerr << "[warn] " << notRect << " slice(s) are not rectangles (the input has "
                  << "non-rectilinear edges); approximated by their bounding boxes\n";
    }
    return mergeStacked(std::move(out));
}

}  // namespace

Blockage buildBlockage(const Design& design, double spacing) {
    Blockage b;
    b.spacing = spacing;

    // (1) The smallest enclosing rectangle is the big blockage. Note this is the whole
    //     bounding box, not the union of the shapes -- the handout asks for a blockage
    //     "as big as possible", so the empty gaps between metal count as blocked too.
    Polygon big;
    bg::convert(design.envelope(), big);

    // (4) Grow each pin by `spacing` and union the results into the region to carve out
    for (const Shape& s : design.shapes) {
        if (!s.isPin()) continue;

        // A pin flush against the boundary grows past the bounding box; that excess
        // is meaningless because the blockage stops at the boundary anyway.
        MultiPolygon clipped;
        bg::intersection(grow(s.poly, spacing), big, clipped);

        // Two pins closer than 2*spacing produce overlapping holes; union merges them
        MultiPolygon merged;
        bg::union_(b.holes, clipped, merged);
        b.holes = std::move(merged);
    }

    bg::difference(big, b.holes, b.region);

    // (5) Slice into rectangles -- APR tools do this internally regardless
    b.slices = sliceToRects(b.region);
    return b;
}

}  // namespace bpv
