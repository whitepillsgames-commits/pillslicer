#pragma once

// PillSlicer: arc overhangs.
//
// A flat overhang that juts out of the layer below with nothing underneath is normally printed with
// supports. Arc overhangs print it in the air instead: starting from a point on the edge it hangs from,
// concentric arcs are laid one next to the other, each resting on the arc printed just before it, so
// every line has something to stick to although nothing lies below it. Where one fan of arcs cannot
// reach (around a corner, past a notch), a new fan starts from the edge of what has already been
// printed. The arcs replace the walls and infill of that area on the first overhanging layer; the
// layers above are printed normally on top of them.

#include <cstddef>

#include "libslic3r.h"
#include "ExPolygon.hpp"
#include "Polyline.hpp"

namespace Slic3r {

class Layer;

struct ArcOverhangParams
{
    // Distance between neighbouring arcs, i.e. the extrusion spacing.
    coord_t spacing            { scaled<coord_t>(0.4) };
    // Arc centerlines stay this far inside the outline of the slice, so the part keeps its size.
    coord_t outline_inset      { scaled<coord_t>(0.2) };
    // Arcs reach this far back over the supported part, so their ends are anchored.
    coord_t anchor_overlap     { scaled<coord_t>(0.4) };
    // Overhangs narrower than this are left to the regular walls, which handle thin overhangs well.
    coord_t min_feature        { scaled<coord_t>(2.) };
    // Chord length used to approximate the circles.
    coord_t max_segment_length { scaled<coord_t>(0.5) };
    // Upper bound on the number of arc fans per overhang, against degenerate shapes.
    size_t  max_centers        { 32 };
};

// Parts of `slice` that hang out of `supported` (the layer below) far enough to need arcs, and that
// hang from a single stretch of supported edge. Areas held at two or more places are bridges, which
// the bridge infill prints better, so they are not returned.
ExPolygons arc_overhang_regions(const ExPolygons &slice, const ExPolygons &supported, const ArcOverhangParams &params);

// Arcs covering `overhang`, in print order. The first ones rest on `supported`, every later one lies
// next to an arc printed before it. `slice` bounds where arcs may go.
Polylines generate_arc_overhang(const ExPolygon &overhang, const ExPolygons &slice, const ExPolygons &supported, const ArcOverhangParams &params);

// Replaces the walls and infill of every arc overhang region of `layer` with arcs, for the regions
// that enable arc_overhangs. Runs after the layer's infill has been generated.
void make_arc_overhangs(Layer &layer);

} // namespace Slic3r
