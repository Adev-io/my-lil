// Coral. Branches that wander, split and sometimes end in a ball.
// My lil Synthscope preset. Ari Klopfer / A_Development, MIT.
set maxdepth 45
set maxobjects 12000
set background #1a0d12

{ color coral } grow

rule grow w 6 {
  { s 0.4 1 0.4 } box
  { y 0.8 rx 9 ry 23 s 0.93 h 2 } grow
}
rule grow w 1 {
  { rz 35 } grow
  { rz -35 ry 90 } grow
}
rule grow w 0.3 { { s 1.2 } sphere }
