
set multiplot layout 2,2 columnsfirst title "{/:Bold=15 Simplex noise 4D}"
set view equal xyz
unset colorbox
# unset border
# unset xtics
# unset ytics
# unset ztics

splot "snoise_4d_0.txt" using 1:2:3:4 w p ps 0.5 pt 7 lc palette z notitle
splot "snoise_4d_1.txt" using 1:2:3:4 w p ps 0.5 pt 7 lc palette z notitle
splot "snoise_4d_2.txt" using 1:2:3:4 w p ps 0.5 pt 7 lc palette z notitle
splot "snoise_4d_3.txt" using 1:2:3:4 w p ps 0.5 pt 7 lc palette z notitle