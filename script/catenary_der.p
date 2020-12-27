
set terminal x11
set output
set title 'Catenary'

plot "catenary_der.txt" using 1:2 with lp lt rgb "blue" lw 1 title "catenary",\
	 "catenary_der.txt" using 1:2:3:4 with vectors head filled lt rgb "orange" title "tangent"
