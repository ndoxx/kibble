
set multiplot layout 1,2 columnsfirst title "{/:Bold=15 Simplex noise}"

set view map
set dgrid3d
set pm3d interpolate 10,10
splot "snoise_2d.txt" using 1:2:3 with pm3d title "2D simplex noise"
unset pm3d
unset dgrid3d
unset view

splot "snoise_3d.txt" using 1:2:3:4 w p ps 0.75 pt 7 lc palette z title "3D simplex noise"