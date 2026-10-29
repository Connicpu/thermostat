#!/bin/bash
# OpenSCAD previews of the case. Run from anywhere; writes PNGs next to this script.
set -euo pipefail
cd "$(dirname "$0")"
render() {
  local part="$1" out="$2" cam="$3" proj="${4:-ortho}" size="${5:-1600,1000}"
  xvfb-run -a openscad -o "$out" --imgsize="$size" --projection="$proj" \
    --camera="$cam" -D "part=\"$part\"" thermostat_case.scad
}
# look-at x,y,z, rot x,y,z, distance
render backplate        case-backplate.png       73.5,35.5,8,0,0,0,250
render shell            case-back.png            73.5,35.5,13,0,0,0,250
render shell            case-back-angle.png      73.5,35.5,16,58,0,32,420  p 1600,1100
render section          case-section.png         73.5,12,13,90,0,0,340
render section_j1       case-back-section.png    73.5,4,13,90,0,0,240
render section_airflow  case-section-airflow.png 73.5,18,13,90,0,0,320
render exploded         case-exploded.png        78,28,48,60,0,28,560      p
render assembly         case-assembled.png       78,28,40,58,0,28,520      p
render assembly         case-assembled-zoom.png  100,40,28,62,0,18,260     p
render assembly         case-front.png           73.5,35.5,30,72,0,0,380   p
render shell            case-front-swirls-bold.png 73.5,35.5,28,70,0,0,360 p
render section_j1       case-section-j1.png      66,4,12,90,0,0,110
render section_j1x      case-section-j1-c43.png  110,35,14,90,0,90,260
render shell            case-snap-window.png     38,4,6,68,0,10,90         p
render assembly         case-curl-zoom.png       112,30,22,55,0,70,160     p
echo done
