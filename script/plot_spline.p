set terminal x11
set output
set title 'Spline'

set xrange [-1:5]
set yrange [-1:5]
plot "spline.txt" using 2:3 with line lt rgb "blue" lw 4 title "curve", \
	 "spline.txt" using 2:3:4:5 with vectors head filled lt rgb "orange" title "first derivative", \
	 "spline.txt" using 2:3:6:7 with vectors head filled lt rgb "red" title "second derivative"