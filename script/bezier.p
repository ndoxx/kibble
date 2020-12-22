set terminal x11
set output
set title 'Bezier'

set xrange [0:2.5]
set yrange [0:2]
plot "bezier.txt" using 2:3 with linespoints, "bezier.txt" using 2:3:4:5 with vectors head filled lt 1