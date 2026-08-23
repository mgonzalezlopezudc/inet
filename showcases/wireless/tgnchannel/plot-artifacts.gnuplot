set datafile separator comma
set terminal pngcairo size 1200,700
set key outside
set grid

set output 'results/tgn-frequency-response.png'
set title 'Deterministic TGn wideband response at t = 4 ms'
set xlabel 'Baseband frequency offset (MHz)'
set ylabel '|H(f)|^2'
plot 'results/tgn-frequency-response.csv' using ($1/1e6):5 every ::1 with lines title 'SISO power gain'

set output 'results/tgn-time-evolution.png'
set title 'Deterministic TGn response at +1 MHz'
set xlabel 'Absolute simulation time (ms)'
set ylabel '|H(t)|^2'
plot 'results/tgn-time-evolution.csv' using ($1*1e3):5 every ::1 with lines title 'SISO power gain'

set output 'results/tgn-capacity-cdf.png'
set title 'TGn 4x4 capacity CDF at 10 dB average SNR'
set xlabel 'Capacity (bit/s/Hz)'
set ylabel 'Cumulative probability'
plot for [name in 'A B C D E F iid'] 'results/tgn-capacity-cdf.csv' using (strcol(1) eq name ? $3 : 1/0):2 every ::1 with lines title name
