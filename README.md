# BPV — Blockage, Pin and Via Abstract View Generator

DA HW1 / Project-1. A C++17 program that turns a cell layout description into the
abstract view an auto-place-and-route (APR) tool needs, using Boost.Geometry's
Boolean operations.

## Quick start

```bash
git clone https://github.com/CalvinKuan/DA_HW1.git
cd DA_HW1

# One-time setup: fetch the Boost headers (~180 MB, header-only, nothing to compile).
# Skip this entirely if Boost is already installed on this machine -- see below.
mkdir -p third_party
curl -sL https://archives.boost.io/release/1.88.0/source/boost_1_88_0.tar.gz \
  | tar xz -C third_party boost_1_88_0/boost
mv third_party/boost_1_88_0/boost third_party/boost && rmdir third_party/boost_1_88_0

make && make run
open out/case1_A.svg out/case1_B.svg     # xdg-open on Linux
```

**If Boost is already on the system include path** (the CentOS7 machine, or any box
with `boost-devel` installed), skip the download and point the build at it instead:

```bash
git clone https://github.com/CalvinKuan/DA_HW1.git
cd DA_HW1
make BOOST_INC=/usr/include && make run
```

`make` without the Boost headers in place fails with
`fatal error: 'boost/geometry.hpp' file not found` -- that means the one-time setup
above has not been done, not that anything is broken.

View the SVGs in a browser. Avoid macOS Quick Look and Finder thumbnails: they force
a non-square SVG into a square box and clip the bottom, which looks like a bug in the
drawing but is not.

## Background

APR tools do not need a cell's base layers (poly, diffusion). They need to know how
big the cell is, where its pins are, and where routing is forbidden. That reduced
description is the *abstract view*; LEF files carry essentially this data, and
Synopsys calls the step that produces it **BPV** — blockage, pin and via.

Inside a standard cell, M1 is already taken up by intra-cell wiring and the power
rails. If the router were to drop another M1 wire over the cell it would short the
internal nets, so the whole cell footprint is declared as an M1 blockage and holes
are punched only where the pins are. The router then comes in on M2 and drops a via
down onto each pin. This program produces that blockage and those holes.

## What it does

1. **Parse** a layout description: layers, polygons, rectangles and text labels.
2. **Find the smallest enclosing rectangle** of every object. This bounding box
   becomes the big blockage — note it is the whole box, not the union of the shapes,
   because a blockage should be as large as possible.
3. **Identify pins** from the text labels: a shape touched by a label becomes a pin,
   named after that label.
4. **Carve the pins out** of the blockage, leaving a 0.5 minimum spacing around each
   one so a via has legal room to land.
5. **Slice** the remaining blockage into rectangles, which is what an APR tool would
   do internally anyway.

It writes two SVG figures: **(A)** the layout exactly as parsed, and **(B)** the
carved and sliced blockage.

## Build options

Boost.Geometry is header-only, so no Boost libraries are compiled or linked.
`third_party/` is not committed, which is why the headers are fetched separately.

| Command | What it does |
| --- | --- |
| `make` | Build `./bpv` using the headers under `third_party/` |
| `make BOOST_INC=/usr/include` | Build against a system-installed Boost |
| `make run` | Build, then run `cases/case1.txt` into `out/` |
| `make parse-check` | Run `cases/case0.txt`, which exercises the parser only |
| `make clean` | Remove objects, the binary and `out/` |

Requires a C++17 compiler. Tested with Apple clang 17 and Boost 1.88.

## Usage

```
./bpv <input case> [output prefix] [min spacing]
```

```bash
./bpv cases/case1.txt out/case1 0.5
```

The min spacing defaults to 0.5, the value the assignment specifies for M1. Output
goes to `<prefix>_A.svg` and `<prefix>_B.svg`.

## Input format

Lines whose first non-blank character is `#` are comments. Blank lines are ignored.
Commas are separators, equivalent to whitespace.

```
number_of_layers 1
M1  1
end_of_layer

number_of_polygons 5
1    0  0,  0 10,  10 10,  10 0,  0  0      # layer, then a closed point list
...
end_of_polygon

number_of_rectangles 3
1  0 20,  3 24                              # layer, then two opposite corners
...
end_of_rectangle

number_of_texts 3
1  C   1  21                                # layer, label, location
...
end_of_text
```

Note the section terminators are singular (`end_of_layer`) while the counts are
plural (`number_of_layers`). Polygons may be given in either winding order, with or
without a repeated closing point. Diagonal edges and negative coordinates are
accepted.

## Result for the provided test case

```
== parse cases/case1.txt ==
layers       : 1
               M1 = 1
shapes       : 8
texts        : 3
bounding box : (0, 0) - (40, 50)   area = 2000
pins         :
               line 16  C  area = 12
               line 17  A  area = 16
               line 18  B  area = 6

== blockage (min spacing = 0.5) ==
carved holes : area = 50
blockage     : area = 1950   (= 2000 - 50)
sliced into  : 7 rectangles
               (0, 0) - (40, 9.5)         area = 380
               (0, 9.5) - (37.5, 13.5)    area = 150
               (0, 13.5) - (40, 19.5)     area = 240
               (3.5, 19.5) - (40, 24.5)   area = 182.5
               (0, 24.5) - (40, 45.5)     area = 840
               (0, 45.5) - (12.5, 50)     area = 56.25
               (17.5, 45.5) - (40, 50)    area = 101.25
slice total  : 1950   (should equal the blockage area)
```

The slice total matching the blockage area is a self-check: the rectangles tile the
region exactly, with no gaps and no double counting.

Pre-built figures are committed under [`docs/`](docs/) so they can be viewed without
building: [figure A](docs/case1_A.svg), [figure B](docs/case1_B.svg).

## Project layout

```
src/bpv.h          shared types: Shape, Text, Design, Blockage
src/parser.cpp     the parser, envelope(), findPins()
src/blockage.cpp   buildBlockage(): buffer -> union -> difference -> scan-line slicing
src/draw.cpp       SVG output via Boost's svg_mapper
src/main.cpp       entry point and reporting
cases/case1.txt    the assignment's test case
cases/case0.txt    the handout's format example; parser-only, run via `make parse-check`
```

Boost.Geometry APIs used: `envelope`, `expand`, `correct`, `is_valid`, `covered_by`,
`buffer`, `union_`, `intersection`, `difference`, `area`, `for_each_point`,
`svg_mapper`.

## Implementation notes

**The blockage is the bounding box, not the union of the shapes.** The empty gaps
between metal are blocked too. The handout asks for a blockage "as big as possible":
a larger blockage only makes the router more conservative, and it describes the cell
in far fewer rectangles than a shape-hugging outline would.

**Pins are grown with a miter join, not a round one.** A strict DRC spacing rule
measures the shortest distance between two shapes, which rounds corners. Layout and
LEF conventionally expand rectangles squarely, and round corners would not slice into
clean rectangles.

**Slices are merged after cutting.** Boost's `buffer` leaves collinear vertices where
the original corners were — pin B at `(38,10)-(40,13)` still has vertices at `y=10`
and `y=13` after being grown, even though those sit partway along a vertical edge.
The scan line treats each as a cut, yielding 11 correct-but-redundant pieces instead
of 7. Merging vertically adjacent slices that span the same x range afterwards is
sturdier than trying to detect collinearity up front.

**Scan-line slicing is exact only for rectilinear geometry.** That covers the
assignment's data and the vast majority of real M1. Diagonal edges make a band yield
a trapezoid; the code then falls back to that piece's bounding box and warns. Erring
large is the safe direction — a blockage that is too small can let the router run
into existing metal.

**`covered_by` is used for pin detection**, so a label sitting exactly on a shape's
border counts as touching it. Switch to `bg::within` in `findPins()` for strict
interior containment.

**A label may mark more than one shape**, so `Shape::pinNames` is a vector. In real
LEF a single pin can have several PORT shapes.

## Not implemented

The "V" of BPV — precomputing legal via landing positions on each pin. The assignment
asks only for the blockage and pin steps, and the test case has a single layer, so
there is no M2 to drop a via from.

## Development note

Written with AI assistance (Anthropic's Claude). The implementation notes above
record the reasoning behind each non-obvious choice, and the geometry was checked
against independently hand-computed values: a bounding box of 2000, holes of 50, a
blockage of 1950, and 7 slices whose areas sum back to 1950.
