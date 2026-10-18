// parser.cpp -- read the layer / polygon / rectangle / text description file,
// then identify pins from the text labels.
#include "bpv.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace bpv {
namespace {

// Drop \r and surrounding whitespace. A full-line comment (first non-blank
// character is '#') and a blank line both come back as an empty string.
std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    const std::size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos || s[b] == '#') return "";
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

// Fetch the next line with content, skipping blanks and comments
bool nextLine(std::istream& in, std::string& out, int& lineNo) {
    std::string raw;
    while (std::getline(in, raw)) {
        ++lineNo;
        out = strip(raw);
        if (!out.empty()) return true;
    }
    return false;
}

// Read every comma-separated coordinate on a line. A comma is just a separator,
// equivalent to whitespace.
std::vector<double> readCoords(const std::string& rest, int lineNo) {
    std::string t = rest;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::istringstream ss(t);

    std::vector<double> v;
    double x = 0;
    while (ss >> x) v.push_back(x);

    // The loop either ran out of input or hit something that is not a number
    ss.clear();
    std::string junk;
    if (ss >> junk) throw ParseError(lineNo, "unparsable token in coordinate list: " + junk);
    if (v.size() % 2 != 0) throw ParseError(lineNo, "odd number of coordinates; cannot form (x y) pairs");
    return v;
}

void checkCount(const char* what, int declared, int got, int lineNo) {
    if (declared != got) {
        std::cerr << "[warn] line " << lineNo << ": declared " << declared << " " << what
                  << "(s), but read " << got << "\n";
    }
}

void parseLayers(std::istream& in, Design& d, int n, int& lineNo) {
    std::string line;
    int got = 0;
    while (nextLine(in, line, lineNo)) {
        if (line == "end_of_layer") { checkCount("layer", n, got, lineNo); return; }

        std::istringstream ls(line);
        std::string name;
        int num = 0;
        if (!(ls >> name >> num))
            throw ParseError(lineNo, "a layer entry must be <name> <number>: " + line);

        d.layerName[num]  = name;
        d.layerNum[name]  = num;
        ++got;
    }
    throw ParseError(lineNo, "reached end of file without end_of_layer");
}

void parsePolygons(std::istream& in, Design& d, int n, int& lineNo) {
    std::string line;
    int got = 0;
    while (nextLine(in, line, lineNo)) {
        if (line == "end_of_polygon") { checkCount("polygon", n, got, lineNo); return; }

        std::istringstream ls(line);
        Shape s;
        s.kind    = Shape::kPolygon;
        s.srcLine = lineNo;
        if (!(ls >> s.layer))
            throw ParseError(lineNo, "a polygon must start with a layer number: " + line);

        std::string rest;
        std::getline(ls, rest);
        const std::vector<double> v = readCoords(rest, lineNo);
        if (v.size() < 6) throw ParseError(lineNo, "a polygon needs at least 3 points");

        for (std::size_t i = 0; i < v.size(); i += 2)
            s.poly.outer().emplace_back(v[i], v[i + 1]);

        // The input may or may not repeat the first point, and may wind either way.
        // correct() closes the ring and fixes the orientation Boost expects.
        bg::correct(s.poly);
        bg::envelope(s.poly, s.box);

        std::string why;
        if (!bg::is_valid(s.poly, why))
            std::cerr << "[warn] line " << lineNo << ": polygon is not a valid geometry ("
                      << why << ")\n";

        d.shapes.push_back(std::move(s));
        ++got;
    }
    throw ParseError(lineNo, "reached end of file without end_of_polygon");
}

void parseRects(std::istream& in, Design& d, int n, int& lineNo) {
    std::string line;
    int got = 0;
    while (nextLine(in, line, lineNo)) {
        if (line == "end_of_rectangle") { checkCount("rectangle", n, got, lineNo); return; }

        std::istringstream ls(line);
        Shape s;
        s.kind    = Shape::kRect;
        s.srcLine = lineNo;
        if (!(ls >> s.layer))
            throw ParseError(lineNo, "a rectangle must start with a layer number: " + line);

        std::string rest;
        std::getline(ls, rest);
        const std::vector<double> v = readCoords(rest, lineNo);
        if (v.size() != 4)
            throw ParseError(lineNo, "a rectangle needs exactly 2 points (x1 y1, x2 y2)");

        // Do not assume the input gives lower-left then upper-right; normalise it
        const double x1 = std::min(v[0], v[2]), x2 = std::max(v[0], v[2]);
        const double y1 = std::min(v[1], v[3]), y2 = std::max(v[1], v[3]);
        if (x1 == x2 || y1 == y2) throw ParseError(lineNo, "rectangle has zero area");

        s.box = Box(Point(x1, y1), Point(x2, y2));
        bg::convert(s.box, s.poly);   // keep a polygon form too, for the Boolean ops later

        d.shapes.push_back(std::move(s));
        ++got;
    }
    throw ParseError(lineNo, "reached end of file without end_of_rectangle");
}

void parseTexts(std::istream& in, Design& d, int n, int& lineNo) {
    std::string line;
    int got = 0;
    while (nextLine(in, line, lineNo)) {
        if (line == "end_of_text") { checkCount("text", n, got, lineNo); return; }

        std::istringstream ls(line);
        Text t;
        t.srcLine = lineNo;
        double x = 0, y = 0;
        if (!(ls >> t.layer >> t.text >> x >> y))
            throw ParseError(lineNo, "a text entry must be <layer> <text> <x> <y>: " + line);

        t.loc = Point(x, y);
        d.texts.push_back(std::move(t));
        ++got;
    }
    throw ParseError(lineNo, "reached end of file without end_of_text");
}

}  // namespace

Box Design::envelope() const {
    if (shapes.empty() && texts.empty())
        throw std::runtime_error("the design is empty; cannot compute a bounding box");

    // A reverse-initialised box works as an accumulator that bg::expand widens
    Box bb;
    bg::assign_inverse(bb);
    for (const Shape& s : shapes) bg::expand(bb, s.box);
    for (const Text& t : texts)   bg::expand(bb, t.loc);
    return bb;
}

Design readDesign(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in) throw std::runtime_error("cannot open: " + path);

    Design d;
    std::string line;
    int lineNo = 0;
    while (nextLine(in, line, lineNo)) {
        std::istringstream hs(line);
        std::string key;
        int n = 0;
        hs >> key >> n;

        if      (key == "number_of_layers")     parseLayers(in, d, n, lineNo);
        else if (key == "number_of_polygons")   parsePolygons(in, d, n, lineNo);
        else if (key == "number_of_rectangles") parseRects(in, d, n, lineNo);
        else if (key == "number_of_texts")      parseTexts(in, d, n, lineNo);
        else throw ParseError(lineNo, "unrecognised section header: " + key);
    }
    return d;
}

int findPins(Design& design) {
    int orphans = 0;
    for (Text& t : design.texts) {
        bool hit = false;
        for (Shape& s : design.shapes) {
            if (s.layer != t.layer) continue;              // a label only marks shapes on its own layer
            if (!bg::covered_by(t.loc, s.poly)) continue;  // covered_by: landing on the border counts
            s.pinNames.push_back(t.text);
            hit = true;
        }
        t.onPin = hit;
        if (!hit) {
            ++orphans;
            std::cerr << "[warn] line " << t.srcLine << ": text '" << t.text
                      << "' does not touch any shape on layer " << t.layer << "\n";
        }
    }
    return orphans;
}

}  // namespace bpv
