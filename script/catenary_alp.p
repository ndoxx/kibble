
set terminal x11
set output
set title 'Catenary'

# set xrange [0:2]
# set yrange [0:3]

plot "catenary_alp.txt" using 1:2 with lp lt rgb "blue" pt 7 ps 1 lw 1 title "raw param",\
	 "catenary_alp.txt" using 3:4 with lp lt rgb "red" pt 7 ps 1 lw 1 title "arc-length param"
