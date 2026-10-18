// main.cpp -- DA HW1 / Project-1 (BPV) entry point
//
//   (1) parser + smallest enclosing rectangle (= the big blockage)
//   (2) identify pins from the text labels
//   (3) carve the pins out of the big blockage, grown by the 0.5 min spacing
//   (A) draw the layout exactly as parsed
//   (B) draw the carved blockage, sliced into rectangles
#include "bpv.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <input case> [output prefix] [min spacing]\n"
                  << "example: " << argv[0] << " cases/case1.txt out/case1 0.5\n";
        return 1;
    }

    const std::string input   = argv[1];
    const std::string prefix  = (argc >= 3) ? argv[2] : "out";
    const double      spacing = (argc >= 4) ? std::atof(argv[3]) : 0.5;  // assignment: 0.5 for M1

    try {
        bpv::Design design = bpv::readDesign(input);
        bpv::findPins(design);

        const bpv::Box bb = design.envelope();

        std::cout << "== parse " << input << " ==\n";
        std::cout << "layers       : " << design.layerName.size() << "\n";
        for (const auto& kv : design.layerName)
            std::cout << "               " << kv.second << " = " << kv.first << "\n";
        std::cout << "shapes       : " << design.shapes.size() << "\n";
        std::cout << "texts        : " << design.texts.size() << "\n";
        std::cout << "bounding box : (" << bpv::bg::get<bpv::bg::min_corner, 0>(bb) << ", "
                                        << bpv::bg::get<bpv::bg::min_corner, 1>(bb) << ") - ("
                                        << bpv::bg::get<bpv::bg::max_corner, 0>(bb) << ", "
                                        << bpv::bg::get<bpv::bg::max_corner, 1>(bb) << ")"
                  << "   area = " << bpv::bg::area(bb) << "\n";

        std::cout << "pins         :\n";
        for (const bpv::Shape& s : design.shapes) {
            if (!s.isPin()) continue;
            std::cout << "               line " << s.srcLine << "  ";
            for (const std::string& n : s.pinNames) std::cout << n << " ";
            std::cout << " area = " << bpv::bg::area(s.poly) << "\n";
        }

        const std::string svgA = prefix + "_A.svg";
        bpv::writeParsedSvg(design, svgA);
        std::cout << "wrote        : " << svgA << "   (A) parsed layout\n";

        const bpv::Blockage block = bpv::buildBlockage(design, spacing);

        std::cout << "\n== blockage (min spacing = " << spacing << ") ==\n";
        std::cout << "carved holes : area = " << bpv::bg::area(block.holes) << "\n";
        std::cout << "blockage     : area = " << bpv::bg::area(block.region)
                  << "   (= " << bpv::bg::area(bb) << " - "
                  << bpv::bg::area(block.holes) << ")\n";
        std::cout << "sliced into  : " << block.slices.size() << " rectangles\n";

        double sum = 0;
        for (const bpv::Box& r : block.slices) {
            std::cout << "               ("
                      << bpv::bg::get<bpv::bg::min_corner, 0>(r) << ", "
                      << bpv::bg::get<bpv::bg::min_corner, 1>(r) << ") - ("
                      << bpv::bg::get<bpv::bg::max_corner, 0>(r) << ", "
                      << bpv::bg::get<bpv::bg::max_corner, 1>(r) << ")"
                      << "   area = " << bpv::bg::area(r) << "\n";
            sum += bpv::bg::area(r);
        }
        std::cout << "slice total  : " << sum << "   (should equal the blockage area)\n";

        const std::string svgB = prefix + "_B.svg";
        bpv::writeBlockageSvg(design, block, svgB);
        std::cout << "wrote        : " << svgB << "   (B) carved and sliced blockage\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
