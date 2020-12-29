
set multiplot layout 2,2 columnsfirst title "{/:Bold=15 Simplex noise}"
set size square
set view map
set dgrid3d
set pm3d interpolate 10,10
unset colorbox

set title "2D simplex noise - smoothed"
splot "snoise_smooth_2d.txt" using 1:2:3 with pm3d notitle
set title "2D simplex noise - octaves"
splot "snoise_oct_2d.txt" using 1:2:3 with pm3d notitle 
set title "2D horizontal marble noise"
splot "snoise_marx_2d.txt" using 1:2:3 with pm3d notitle 
set title "2D vertical marble noise"
splot "snoise_mary_2d.txt" using 1:2:3 with pm3d notitle 

unset pm3d
unset dgrid3d
unset view

