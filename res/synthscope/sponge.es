// Sponge. A cube carved into the twenty sub-cubes on its edges, again and again.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define levels 3 (int:1-4)
set background #16161a

{ color #c8ff3a } sponge

rule sponge md levels > solid {
  { s 1/3 y -1 h 20 } ring
  { s 1/3 y 1 h 20 } ring
  { s 1/3 h 20 } corners
}

// the eight cells around a layer's middle
rule ring {
  { x -1 z -1 } sponge  { z -1 } sponge  { x 1 z -1 } sponge
  { x -1 } sponge                         { x 1 } sponge
  { x -1 z 1 } sponge   { z 1 } sponge   { x 1 z 1 } sponge
}

// the middle layer keeps only its four corner columns
rule corners {
  { x -1 z -1 } sponge  { x 1 z -1 } sponge
  { x -1 z 1 } sponge   { x 1 z 1 } sponge
}

rule solid { box }
