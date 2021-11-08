
set multiplot layout 1,3 columnsfirst title "{/:Bold=15 Catenary}"
set key left top
set size square

set title 'Different lengths, same anchors'
plot "catenary.txt" using 1:2 with line lt rgb "blue" lw 1 title "s=1.93576",\
	 "catenary.txt" using 1:3 with line lt rgb "cyan" lw 1 title "s=3.22626",\
	 "catenary.txt" using 1:4 with line lt rgb "green" lw 1 title "s=4.51676",\
	 "catenary.txt" using 1:5 with line lt rgb "red" lw 1 title "s=5.80727"

set title 'Arc-length parameterization'
plot "catenary_alp.txt" using 1:2 with lp lt rgb "blue" pt 7 ps 1 lw 1 title "raw param",\
	 "catenary_alp.txt" using 3:4 with lp lt rgb "red" pt 7 ps 1 lw 1 title "arc-length param"

set title 'Tangent vectors'
plot "catenary_der.txt" using 1:2 with lp lt rgb "blue" lw 1 title "catenary",\
	 "catenary_der.txt" using 1:2:3:4 with vectors head filled lt rgb "orange" title "tangent"

unset multiplot