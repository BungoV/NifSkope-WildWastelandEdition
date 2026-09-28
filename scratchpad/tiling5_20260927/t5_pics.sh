#!/bin/bash
# TILING5 pictures: Boston (maps1 camera, cells -5,-10..2,-3) and the rural hills (-36,4..-25,15),
# flat colour + lit, today's bake vs the height blend -- eight separate full-size files.
# Each shot takes and releases the run lock itself (t5_shot.sh).  Port: one unused per shot.
set -u
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
P=$HERE/pics; mkdir -p $P/raw
# NifSkope's --port is a UDP bind (src/main.cpp IPCsocket::create); a busy UDP port makes the exe forward
# "NifSkope::open" to it and exit 0 silently. A TCP-free port is not a UDP-free port -> pick by UDP bind (21:25).
free_port() { python -c "import socket;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',0));print(s.getsockname()[1]);s.close()"; }
shot() {   # name sheetdir x0 y0 x1 y1 ortho flat -- up to 4 tries; a try that writes no PNG and an empty log
	# (the exe returned 0 before making a window, cause not found, 22:05) is retried on a fresh port.
	local try
	for try in 1 2 3 4; do
		rm -f "$P/raw/$1.png"
		shot1 "$@"
		[ -s "$P/raw/$1.png" ] && { echo "SHOT $1 try $try"; return 0; }
	done
	echo "SHOT $1 FAILED 4 tries"
}
shot1() {
	local port; port=$(free_port)
	if [ "$8" = 1 ]; then
		# "flat" = the colour sheet unlit: WW_LOD_CHANNEL=12 (raw base colour, texturing on).
		# WW_RENDER_FLAT=1 draws vertex colour only and a .lodl carries none -> a white sheet (21:19).
		SDIM=4 WW_LOD_CHANNEL=12 bash $HERE/t5_shot.sh "$P/raw/$1.png" "$2" $3 $4 $5 $6 $7 1600 1565 $port
	else
		SDIM=4 bash $HERE/t5_shot.sh "$P/raw/$1.png" "$2" $3 $4 $5 $6 $7 1600 1565 $port
	fi
}
# the .lodt pyramid (Commonwealth.VT.*.lodt) is what the view reads -- it lives in mod/, not tex/ (fixed 21:20)
V=mod/FO4CSLOD/Commonwealth
BB=$HERE/out/id_c2/boston/$V;       BA=$HERE/out/fixB16/boston/$V
RB=$HERE/out/today/r_-36_4_-25_15/$V; RA=$HERE/out/fix/r_-36_4_-25_15/$V
for only in ${ONLY:-1 2 3 4 5 6 7 8}; do
	case $only in
	1) shot boston_flat_today  $BB -5 -10 2 -3 16384 1 ;;
	2) shot boston_flat_height $BA -5 -10 2 -3 16384 1 ;;
	3) shot boston_lit_today   $BB -5 -10 2 -3 16384 0 ;;
	4) shot boston_lit_height  $BA -5 -10 2 -3 16384 0 ;;
	5) shot rural_flat_today   $RB -36 4 -25 15 24576 1 ;;
	6) shot rural_flat_height  $RA -36 4 -25 15 24576 1 ;;
	7) shot rural_lit_today    $RB -36 4 -25 15 24576 0 ;;
	8) shot rural_lit_height   $RA -36 4 -25 15 24576 0 ;;
	esac
done
