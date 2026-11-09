// Stairs. Eighty steps winding up a spiral.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define rise 0.5 (float:0.1-1.5)
#define turn 9 (float:0-30)
set background #f2f2f4

{ color #ff3d9a } 80 * { y rise ry turn h 4 } step

rule step { { x 3 s 2 0.3 1 } box }
