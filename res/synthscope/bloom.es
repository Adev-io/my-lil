// Bloom. Twelve curling petals around a stem.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define curl 8 (float:0-30)
#define taper 0.96 (float:0.9-0.99)
set maxdepth 120
set background #120818

{ color #ff3d9a } 12 * { ry 30 } petal
{ color #c8ff3a s 0.4 4 0.4 y -0.6 } box

rule petal {
  { s 1 0.08 0.5 } box
  { x 0.6 rz curl s taper h 2 } petal
}
