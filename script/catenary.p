
set terminal x11
set output
set title 'Catenary'

# set xrange [0:2]
# set yrange [0:3]

plot "catenary.txt" using 1:2 with lp lt rgb "blue" lw 1 title "catenary 1",\
	 "catenary.txt" using 1:3 with lp lt rgb "cyan" lw 1 title "catenary 2",\
	 "catenary.txt" using 1:4 with lp lt rgb "green" lw 1 title "catenary 3",\
	 "catenary.txt" using 1:5 with lp lt rgb "red" lw 1 title "catenary 4"