// Cube Lattice. An 8 x 8 x 8 grid where each cell is a box, a ball, or nothing.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define gap 1.4 (float:1-3)
set background #16161a

{ color #ff3d9a } lattice

rule lattice { 8 * { x gap h 12 } 8 * { y gap h 6 } 8 * { z gap b 0.96 } cell }

rule cell w 3 { { s 0.8 } box }
rule cell w 1 { { s 0.6 } sphere }
rule cell w 1 { }
