// Fern. A stem that now and then forks into two smaller stems.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
#define fork 40 (float:10-80)
set maxdepth 70
set maxobjects 15000
set background #0d1a10

{ color #c8ff3a } stem

rule stem w 10 {
  seg
  { y 0.9 s 0.96 rz 4 h 1 } stem
}

rule stem w 1 {
  seg
  { y 0.9 s 0.96 rz 4 } stem
  { y 0.9 rz fork s 0.6 h 10 } stem
  { y 0.9 rz -fork s 0.6 ry 90 h -10 } stem
}

rule seg { { s 0.15 1 0.15 } box }
