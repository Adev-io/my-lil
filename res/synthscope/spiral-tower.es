// Spiral Tower. A slab twisting up a shrinking column.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define twist 12 (float:0-40)
#define shrink 0.97 (float:0.9-0.995)
set maxdepth 300
set background #101018

{ color #5ac8e6 } tower

rule tower {
  slab
  { y 0.9 ry twist s shrink h 3 } tower
}

rule slab { { s 3 0.15 0.5 } box }
