// bpv.h -- Blockage / Pin / Via abstract-view generator: shared types and interface
#ifndef BPV_H
#define BPV_H

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace bpv {

namespace bg = boost::geometry;

// Coordinates are double: a min spacing of 0.5 produces non-integer coordinates.
using Point        = bg::model::d2::point_xy<double>;
using Ring         = bg::model::ring<Point>;
using Polygon      = bg::model::polygon<Point>;
using MultiPolygon = bg::model::multi_polygon<Polygon>;
using Box          = bg::model::box<Point>;

// One piece of metal. Both polygons and rectangles are stored as a Polygon so the
// Boolean operations later do not need two separate code paths.
struct Shape {
    enum Kind { kPolygon, kRect };

    int      layer = 0;
    Kind     kind  = kPolygon;
    Polygon  poly;                       // valid for both kinds
    Box      box;                        // meaningful only for kRect
    int      srcLine = 0;                // source line number, used in diagnostics
    std::vector<std::string> pinNames;   // labels touching this shape; non-empty => it is a pin

    bool isPin() const { return !pinNames.empty(); }
};

// A text label. It has no area, only a location; its purpose is to identify pins.
struct Text {
    int         layer = 0;
    std::string text;
    Point       loc;
    int         srcLine = 0;
    bool        onPin = false;   // whether it landed on a shape; if not, it needs dark ink to stay visible
};

// One parsed design
struct Design {
    std::map<int, std::string> layerName;   // 1 -> "M1"
    std::map<std::string, int> layerNum;    // "M1" -> 1
    std::vector<Shape>         shapes;
    std::vector<Text>          texts;

    Box envelope() const;                   // smallest enclosing rectangle of every object
};

// Parse error carrying the offending line number
struct ParseError : std::runtime_error {
    ParseError(int line, const std::string& what)
        : std::runtime_error("line " + std::to_string(line) + ": " + what) {}
};

// --- parser.cpp ---
Design readDesign(const std::string& path);

// Identify pins from the text locations: flag every shape a label touches.
// Returns the number of labels that touched nothing (0 means every label found a home).
int findPins(Design& design);

// Product of requirements (1), (4) and (5)
struct Blockage {
    double           spacing = 0.5;   // min spacing for this layer (the assignment specifies 0.5 for M1)
    MultiPolygon     holes;           // pins grown by spacing, clipped back to the bounding box
    MultiPolygon     region;          // the big blockage with the holes removed
    std::vector<Box> slices;          // region cut into rectangles
};

// --- blockage.cpp ---
// Take the bounding box as the big blockage, carve out (pin + spacing),
// then slice what remains into rectangles.
Blockage buildBlockage(const Design& design, double spacing);

// --- draw.cpp ---
// Requirement (A): the shapes exactly as parsed
void writeParsedSvg(const Design& design, const std::string& path);

// Requirement (B): the carved blockage, sliced into rectangles
void writeBlockageSvg(const Design& design, const Blockage& b, const std::string& path);

}  // namespace bpv

#endif  // BPV_H
