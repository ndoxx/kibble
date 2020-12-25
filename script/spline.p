
set terminal x11
set output
set title 'Spline'

plot "spline.txt" using 2:3 with lp lt rgb "blue" lw 1 title "curve"