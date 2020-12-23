set terminal x11
set output
set title 'Bezier'

set xrange [0:2.5]
set yrange [0:2]
plot "bezier.txt" using 2:3 with line lt rgb "blue" lw 4 title "curve", \
	 "bezier.txt" using 2:3:4:5 with vectors head filled lt rgb "orange" title "first derivative", \
	 "bezier.txt" using 2:3:6:7 with vectors head filled lt rgb "red" title "second derivative"