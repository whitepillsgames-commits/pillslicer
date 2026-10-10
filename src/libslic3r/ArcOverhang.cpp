#include "ArcOverhang.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "ClipperUtils.hpp"
#include "ExtrusionEntity.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "Flow.hpp"
#include "Layer.hpp"
#include "Point.hpp"
#include "Polygon.hpp"
#include "Print.hpp"
#include "PrintConfig.hpp"
#include "Surface.hpp"

namespace Slic3r {

namespace {

double area(const ExPolygons &expolygons)
{
    double a = 0.;
    for (const ExPolygon &expolygon : expolygons)
        a += expolygon.area();
    return a;
}

// Closed circle as a polyline that starts and ends at `start_angle`.
Polyline circle(const Point &center, coord_t radius, double start_angle, coord_t max_segment_length)
{
    const double circumference = 2. * PI * double(radius);
    const size_t segments      = std::max<size_t>(24, size_t(std::ceil(circumference / double(std::max<coord_t>(max_segment_length, 1)))));
    Polyline     out;
    out.points.reserve(segments + 1);
    for (size_t i = 0; i <= segments; ++i) {
        const double a = start_angle + 2. * PI * double(i) / double(segments);
        out.points.emplace_back(center.x() + coord_t(std::round(double(radius) * std::cos(a))),
                                center.y() + coord_t(std::round(double(radius) * std::sin(a))));
    }
    return out;
}

// Number of separate stretches the polylines form once each is thickened by `gap`: pieces closer
// than that count as one stretch.
size_t count_stretches(const Polylines &polylines, coord_t gap)
{
    return polylines.empty() ? 0 : union_ex(offset(polylines, float(gap))).size();
}

// Point of `polylines` closest to `target`.
bool closest_point_on(const Polylines &polylines, const Point &target, Point &out)
{
    double best = std::numeric_limits<double>::max();
    for (const Polyline &polyline : polylines) {
        if (polyline.points.size() < 2)
            continue;
        const Point  p = target.projection_onto(polyline);
        const double d = (p - target).cast<double>().squaredNorm();
        if (d < best) {
            best = d;
            out  = p;
        }
    }
    return best < std::numeric_limits<double>::max();
}

double max_distance(const Point &from, const ExPolygon &expolygon)
{
    double d = 0.;
    for (const Point &p : expolygon.contour.points)
        d = std::max(d, (p - from).cast<double>().norm());
    return d;
}

} // namespace

ExPolygons arc_overhang_regions(const ExPolygons &slice, const ExPolygons &supported, const ArcOverhangParams &params)
{
    // What lies over the layer below, plus half a line: hanging out by less than that is not an overhang.
    const Polygons supported_band = offset(supported, float(params.spacing / 2));
    ExPolygons     candidates     = opening_ex(diff_ex(slice, supported_band), float(params.min_feature / 2));

    ExPolygons out;
    const double min_area = double(params.min_feature) * double(params.min_feature);
    for (ExPolygon &candidate : candidates) {
        if (candidate.area() < min_area)
            continue;
        // The edge the overhang hangs from: its boundary next to the supported part.
        const Polylines anchors = intersection_pl(to_polylines(candidate), offset(supported_band, float(params.spacing)));
        if (count_stretches(anchors, params.spacing) == 1)
            out.emplace_back(std::move(candidate));
    }
    return out;
}

Polylines generate_arc_overhang(const ExPolygon &overhang, const ExPolygons &slice, const ExPolygons &supported, const ArcOverhangParams &params)
{
    const coord_t spacing = std::max<coord_t>(params.spacing, 1);
    // Where arcs may go: the overhang plus a strip of the supported part to anchor on, inside the outline.
    const ExPolygons allowed = intersection_ex(offset_ex(overhang, float(params.anchor_overlap)),
                                               offset(slice, -float(params.outline_inset)));
    // Material an arc can rest on: the supported part first, then every arc printed so far.
    Polygons   covered   = offset(supported, 0.f);
    ExPolygons remaining { overhang };
    Polylines  out;

    for (size_t center_idx = 0; center_idx < params.max_centers; ++center_idx) {
        // Slivers left between fans are too thin to hold an arc; the layer above bridges them.
        remaining = opening_ex(diff_ex(remaining, covered), float(spacing) * 0.75f);
        if (remaining.empty())
            break;
        auto largest = std::max_element(remaining.begin(), remaining.end(),
                                        [](const ExPolygon &a, const ExPolygon &b) { return a.area() < b.area(); });
        const ExPolygon region = *largest;

        // The fan starts on the edge where `region` touches printed material, as close to the middle
        // of the region as possible so the arcs spread over all of it.
        const Polylines frontier = intersection_pl(to_polylines(region), offset(covered, float(spacing)));
        const Point     target   = region.contour.centroid();
        Point           center;
        if (!closest_point_on(frontier, target, center)) {
            // Nothing printed touches this part: it cannot be reached by arcs.
            remaining.erase(largest);
            continue;
        }

        // Start each circle on the side of the printed material, so clipping does not split an arc
        // at the circle's seam.
        const Vec2d  away        = (center - target).cast<double>();
        const double start_angle = away.squaredNorm() > 0. ? std::atan2(away.y(), away.x()) : 0.;

        const ExPolygons fan_area = intersection_ex(allowed, offset(region, float(params.anchor_overlap)));
        const double     max_r    = max_distance(center, region) + double(spacing);
        // An arc must lie next to material printed before it, never stand alone in the air.
        Polygons  reach       = offset(covered, float(spacing) * 1.5f);
        Polylines fan;
        size_t    misses      = 0;
        bool      reverse     = false;
        for (double r = double(spacing); r <= max_r; r += double(spacing)) {
            Polylines pieces = intersection_pl(Polylines{ circle(center, coord_t(r), start_angle, params.max_segment_length) }, fan_area);
            Polylines kept;
            for (Polyline &piece : pieces)
                if (piece.points.size() >= 2 && !intersection_pl(Polylines{ piece }, reach).empty())
                    kept.emplace_back(std::move(piece));
            if (kept.empty()) {
                // A ring can miss between two lobes of the region; give up after two in a row.
                if (++misses >= 2)
                    break;
                continue;
            }
            misses = 0;
            // Alternate the direction ring by ring, so each arc starts near where the previous one ended.
            if (reverse)
                for (Polyline &piece : kept)
                    piece.reverse();
            reverse = !reverse;
            reach = union_(reach, offset(kept, float(spacing) * 1.5f));
            append(fan, kept);
        }
        if (fan.empty()) {
            remaining.erase(remaining.begin() + (largest - remaining.begin()));
            continue;
        }
        covered = union_(covered, offset(fan, float(spacing) * 0.6f));
        append(out, std::move(fan));
    }
    return out;
}

namespace {

// Removes the parts of `entity` that lie inside `cut`. Returns the replacement entities: empty when
// nothing is left, the entity itself (moved into the result) when nothing was cut.
void clip_entity(ExtrusionEntity *entity, const ExPolygons &cut, ExtrusionEntitiesPtr &out)
{
    auto clip_path = [&cut](const ExtrusionPath &path, ExtrusionPaths &result) -> bool {
        const Polyline  polyline = path.polyline.to_polyline();
        const Polylines kept     = diff_pl(Polylines{ polyline }, cut);
        if (kept.size() == 1 && kept.front().points == polyline.points) {
            result.emplace_back(path);
            return false;
        }
        for (const Polyline &piece : kept)
            if (piece.points.size() >= 2)
                result.emplace_back(Polyline3(piece), path);
        return true;
    };

    if (auto *collection = dynamic_cast<ExtrusionEntityCollection *>(entity)) {
        ExtrusionEntitiesPtr entities;
        for (ExtrusionEntity *child : collection->entities)
            clip_entity(child, cut, entities);
        collection->entities = std::move(entities);
        if (collection->entities.empty())
            delete collection;
        else
            out.emplace_back(collection);
        return;
    }

    ExtrusionPaths paths;
    bool           changed = false;
    if (auto *loop = dynamic_cast<ExtrusionLoop *>(entity)) {
        for (const ExtrusionPath &path : loop->paths)
            changed |= clip_path(path, paths);
    } else if (auto *multipath = dynamic_cast<ExtrusionMultiPath *>(entity)) {
        for (const ExtrusionPath &path : multipath->paths)
            changed |= clip_path(path, paths);
    } else if (auto *path = dynamic_cast<ExtrusionPath *>(entity)) {
        changed = clip_path(*path, paths);
    } else {
        out.emplace_back(entity);
        return;
    }

    if (!changed) {
        out.emplace_back(entity);
        return;
    }
    // A cut loop or multipath falls apart into its remaining pieces, kept in their print order.
    for (ExtrusionPath &path : paths)
        out.emplace_back(new ExtrusionPath(std::move(path)));
    delete entity;
}

void clip_collection(ExtrusionEntityCollection &collection, const ExPolygons &cut)
{
    ExtrusionEntitiesPtr entities;
    for (ExtrusionEntity *entity : collection.entities)
        clip_entity(entity, cut, entities);
    collection.entities = std::move(entities);
}

} // namespace

void make_arc_overhangs(Layer &layer)
{
    const Layer *lower = layer.lower_layer;
    if (lower == nullptr || lower->lslices.empty())
        return;
    const PrintObject *object = layer.object();
    // Supports already hold these areas up; arcs are the alternative to them.
    if (object->config().enable_support || object->config().raft_layers > 0)
        return;

    for (LayerRegion *layerm : layer.regions()) {
        if (!layerm->region().config().arc_overhangs.value)
            continue;
        const ExPolygons slice = to_expolygons(layerm->slices.surfaces);
        if (slice.empty())
            continue;

        const Flow        flow = layerm->flow(frExternalPerimeter);
        ArcOverhangParams params;
        params.spacing        = std::max<coord_t>(flow.scaled_spacing(), 1);
        params.outline_inset  = flow.scaled_width() / 2;
        params.anchor_overlap = flow.scaled_width();
        params.min_feature    = 4 * params.spacing;

        const ExPolygons regions = arc_overhang_regions(slice, lower->lslices, params);
        if (regions.empty())
            continue;

        // The arcs take the place of the walls and infill there.
        clip_collection(layerm->perimeters, regions);
        clip_collection(layerm->fills, regions);

        for (const ExPolygon &region : regions) {
            Polylines arcs = generate_arc_overhang(region, slice, lower->lslices, params);
            if (arcs.empty())
                continue;
            // Kept in order: every arc needs the one before it to rest on.
            auto *collection    = new ExtrusionEntityCollection();
            collection->no_sort = true;
            for (Polyline &arc : arcs) {
                // Printed as an overhang wall: slowed down and cooled like any wall hanging in the air.
                auto *path     = new ExtrusionPath(erOverhangPerimeter, flow.mm3_per_mm(), flow.width(), flow.height());
                path->polyline = Polyline3(arc);
                collection->entities.emplace_back(path);
            }
            layerm->fills.entities.emplace_back(collection);
        }
    }
}

} // namespace Slic3r
