// Orbit Rings. Twelve tilted rings of beads around one centre.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define radius 6 (float:2-12)
set background #08080c

{ color #ff3d9a sat 0.8 } 12 * { rx 15 h 30 } ring

rule ring { 36 * { ry 10 } bead }
rule bead { { x radius s 0.5 } sphere }
