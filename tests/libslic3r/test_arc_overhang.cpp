#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "libslic3r/ArcOverhang.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/Polyline.hpp"
#include "libslic3r/libslic3r.h"

using namespace Slic3r;

namespace {

ExPolygon rect(double x0, double y0, double x1, double y1)
{
    return ExPolygon(Polygon({ Point(scaled<coord_t>(x0), scaled<coord_t>(y0)), Point(scaled<coord_t>(x1), scaled<coord_t>(y0)),
                               Point(scaled<coord_t>(x1), scaled<coord_t>(y1)), Point(scaled<coord_t>(x0), scaled<coord_t>(y1)) }));
}

ArcOverhangParams params()
{
    ArcOverhangParams p;
    p.spacing        = scaled<coord_t>(0.42);
    p.outline_inset  = scaled<coord_t>(0.21);
    p.anchor_overlap = scaled<coord_t>(0.45);
    p.min_feature    = 4 * p.spacing;
    return p;
}

double covered_fraction(const ExPolygon &region, const Polylines &arcs, coord_t spacing)
{
    double covered = 0.;
    for (const ExPolygon &e : intersection_ex(ExPolygons{ region }, offset(arcs, float(spacing) * 0.55f)))
        covered += e.area();
    return covered / region.area();
}

} // namespace

// A 40 x 15 mm shelf hanging from a 5 mm deep wall along one long side.
TEST_CASE("Arcs cover a shelf that hangs from one edge", "[ArcOverhang]")
{
    const ExPolygons        slice     { rect(0, 0, 40, 20) };
    const ExPolygons        supported { rect(0, 0, 40, 5) };
    const ArcOverhangParams p = params();

    const ExPolygons regions = arc_overhang_regions(slice, supported, p);
    REQUIRE(regions.size() == 1);
    const Polylines arcs = generate_arc_overhang(regions.front(), slice, supported, p);
    REQUIRE_FALSE(arcs.empty());
    // Only slivers along the outline and between fans stay uncovered.
    CHECK(covered_fraction(regions.front(), arcs, p.spacing) > 0.9);
}

TEST_CASE("Arcs stay inside the outline of the part", "[ArcOverhang]")
{
    const ExPolygons        slice     { rect(0, 0, 30, 30) };
    const ExPolygons        supported { rect(10, 10, 20, 20) };
    const ArcOverhangParams p = params();

    const Polygons inside = offset(slice, -float(p.outline_inset) + float(scaled<coord_t>(0.01)));
    for (const ExPolygon &region : arc_overhang_regions(slice, supported, p))
        for (const Polyline &arc : generate_arc_overhang(region, slice, supported, p))
            CHECK(diff_pl(Polylines{ arc }, inside).empty());
}

// The defining property: nothing is printed in the air without a line next to it to stick to.
TEST_CASE("Every arc lies next to the supported part or to an earlier arc", "[ArcOverhang]")
{
    Polygon l_shape({ Point(0, 0), Point(scaled<coord_t>(30.), 0), Point(scaled<coord_t>(30.), scaled<coord_t>(30.)),
                      Point(scaled<coord_t>(20.), scaled<coord_t>(30.)), Point(scaled<coord_t>(20.), scaled<coord_t>(10.)),
                      Point(0, scaled<coord_t>(10.)) });
    const ExPolygons        slice     { ExPolygon(l_shape) };
    const ExPolygons        supported { rect(0, 0, 8, 10) };
    const ArcOverhangParams p = params();

    const ExPolygons regions = arc_overhang_regions(slice, supported, p);
    REQUIRE(regions.size() == 1);
    const Polylines arcs = generate_arc_overhang(regions.front(), slice, supported, p);
    REQUIRE_FALSE(arcs.empty());

    Polygons printed = offset(supported, 0.f);
    for (size_t i = 0; i < arcs.size(); ++i) {
        const Polygons reach = offset(printed, float(p.spacing) * 1.6f);
        CAPTURE(i);
        CHECK_FALSE(intersection_pl(Polylines{ arcs[i] }, reach).empty());
        printed = union_(printed, offset(arcs[i], float(p.spacing) * 0.5f));
    }
}

// A plate held at both ends is a bridge: the bridge infill prints it better than arcs.
TEST_CASE("A bridge between two supports gets no arcs", "[ArcOverhang]")
{
    const ExPolygons slice     { rect(0, 0, 40, 10) };
    const ExPolygons supported { rect(0, 0, 5, 10), rect(35, 0, 40, 10) };
    CHECK(arc_overhang_regions(slice, supported, params()).empty());
}

TEST_CASE("An overhang narrower than the minimum feature gets no arcs", "[ArcOverhang]")
{
    // The part sticks out 1 mm past the layer below; regular walls print that fine.
    const ExPolygons slice     { rect(0, 0, 20, 21) };
    const ExPolygons supported { rect(0, 0, 20, 20) };
    CHECK(arc_overhang_regions(slice, supported, params()).empty());
}
