// draw.cpp -- write .svg output through Boost.Geometry's svg_mapper
#include "bpv.h"

#include <boost/geometry/io/svg/svg_mapper.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bpv {
namespace {

// One colour per layer. The handout's layer section is commented "defining layers
// and the associated colors", but the format itself carries no colour field, so the
// choice is ours.
const char* layerRgb(int layer) {
    switch (layer) {
        case 1:  return "140,169,221";   // M1 light blue
        case 2:  return "122,186,136";   // M2 green
        case 3:  return "226,165, 95";   // M3 orange
        default: return "170,170,170";
    }
}

std::string shapeStyle(const Shape& s) {
    if (s.isPin()) {
        // Pins are drawn solid and dark to stand apart from ordinary metal
        // (matching the dark blue squares in the handout figure)
        return "fill-opacity:0.95;fill:rgb(43,87,166);"
               "stroke:rgb(22,48,94);stroke-width:1";
    }
    return std::string("fill-opacity:0.70;fill:rgb(") + layerRgb(s.layer) +
           ");stroke:rgb(48,69,107);stroke-width:1";
}

// Match the canvas aspect ratio to the design's own, so the drawing is not squashed
void canvasSize(const Design& d, int& w, int& h) {
    const Box   bb = d.envelope();
    const double dx = bg::get<bg::max_corner, 0>(bb) - bg::get<bg::min_corner, 0>(bb);
    const double dy = bg::get<bg::max_corner, 1>(bb) - bg::get<bg::min_corner, 1>(bb);

    const int kLong = 900;
    if (dx <= 0 || dy <= 0) { w = h = kLong; return; }
    if (dx >= dy) { w = kLong; h = static_cast<int>(kLong * dy / dx); }
    else          { h = kLong; w = static_cast<int>(kLong * dx / dy); }
}

// Labels go on top; shared by both figures.
//
// Vertical centring cannot rely on dominant-baseline:central -- browsers honour it,
// but viewers such as macOS Preview and thumbnail generators ignore it outright and
// the glyph drops to the lower left of its box. Nudging the baseline down by about
// 0.35 of the font size (half a cap height) centres it in every viewer.
template <typename Mapper>
void drawTexts(Mapper& mapper, const Design& design) {
    const double fontPx  = 15.0;
    const double baseAdj = fontPx * 0.35;

    for (const Text& t : design.texts) {
        std::ostringstream st;
        st << "fill:rgb(" << (t.onPin ? "255,255,255" : "40,40,40") << ");"
           << "font-family:Arial,Helvetica,sans-serif;font-size:" << fontPx << "px;"
           << "font-weight:bold;text-anchor:middle";
        mapper.text(t.loc, t.text, st.str(), 0.0, baseAdj);
    }
}

// svg_mapper writes width="100%" height="100%" by default. An SVG without explicit
// dimensions leaves many viewers (previewers, thumbnailers, converters) unable to
// work out the canvas size, so supply a viewBox as well.
std::string widthHeight(int w, int h) {
    std::ostringstream wh;
    wh << "width=\"" << w << "\" height=\"" << h
       << "\" viewBox=\"0 0 " << w << " " << h << "\"";
    return wh.str();
}

}  // namespace

void writeParsedSvg(const Design& design, const std::string& path) {
    std::ofstream svg(path.c_str());
    if (!svg) throw std::runtime_error("cannot write: " + path);

    int w = 0, h = 0;
    canvasSize(design, w, h);

    // The 4th argument is a scale factor: 0.95 leaves a 5% margin so the frame's
    // stroke is not clipped against the canvas edge. The header and footer are
    // written on construction and destruction, so the mapper needs its own scope.
    bg::svg_mapper<Point> mapper(svg, w, h, 0.95, widthHeight(w, h));

    // Add everything first so the mapper knows the extent it has to fit
    for (const Shape& s : design.shapes) mapper.add(s.poly);
    for (const Text&  t : design.texts)  mapper.add(t.loc);

    const Box bb = design.envelope();
    Polygon frame;
    bg::convert(bb, frame);
    mapper.add(frame);

    // Draw the frame first as a backdrop; dashed, to signal it is only a reference
    // and not an object from the input file
    mapper.map(frame, "fill:none;stroke:rgb(130,130,130);stroke-width:1;stroke-dasharray:6,4");

    for (const Shape& s : design.shapes) mapper.map(s.poly, shapeStyle(s));

    drawTexts(mapper, design);
}

void writeBlockageSvg(const Design& design, const Blockage& b, const std::string& path) {
    std::ofstream svg(path.c_str());
    if (!svg) throw std::runtime_error("cannot write: " + path);

    int w = 0, h = 0;
    canvasSize(design, w, h);
    bg::svg_mapper<Point> mapper(svg, w, h, 0.95, widthHeight(w, h));

    for (const Box& r : b.slices) mapper.add(r);
    for (const Shape& s : design.shapes) mapper.add(s.poly);

    // Stroke every slice individually so the cut lines are visible
    // (those are the horizontal dividers in the right half of the handout figure)
    for (const Box& r : b.slices)
        mapper.map(r, "fill-opacity:0.85;fill:rgb(205,205,205);"
                      "stroke:rgb(40,40,40);stroke-width:1");

    // Pins stay on top: a pin is not blockage, it is where a via comes down
    for (const Shape& s : design.shapes) {
        if (!s.isPin()) continue;
        mapper.map(s.poly, "fill-opacity:0.95;fill:rgb(43,87,166);"
                           "stroke:rgb(22,48,94);stroke-width:1");
    }

    drawTexts(mapper, design);
}

}  // namespace bpv
