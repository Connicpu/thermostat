set -e
cd /workspace/thermostat/v3/kicad
rm -rf ../fab/gerbers && mkdir -p ../fab/gerbers
kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts --subtract-soldermask --use-drill-file-origin -o ../fab/gerbers/ thermostat-v3.kicad_pcb >/dev/null
kicad-cli pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o ../fab/gerbers/ thermostat-v3.kicad_pcb >/dev/null
(cd ../fab && rm -f thermostat-v3-gerbers.zip && python3 -c "
import zipfile,os;z=zipfile.ZipFile('thermostat-v3-gerbers.zip','w',zipfile.ZIP_DEFLATED)
[z.write('gerbers/'+f,f) for f in sorted(os.listdir('gerbers'))]")
python3 ../export_jlc.py 2>&1 | grep -v -e assert -e leak
kicad-cli pcb export pdf --layers F.SilkS,F.Mask,Edge.Cuts --mode-single --black-and-white -o /tmp/st.pdf thermostat-v3.kicad_pcb >/dev/null
kicad-cli pcb export pdf --layers B.SilkS,B.Mask,Edge.Cuts --mode-single --mirror --black-and-white -o /tmp/sb.pdf thermostat-v3.kicad_pcb >/dev/null
pdftoppm -png -r 300 -singlefile /tmp/st.pdf ../fab/thermostat-v3-silk-top; pdftoppm -png -r 300 -singlefile /tmp/sb.pdf ../fab/thermostat-v3-silk-bottom
for s in top bottom; do kicad-cli pcb render --side $s -w 2400 -h 1300 --quality high -o ../fab/thermostat-v3-3d-$s.png thermostat-v3.kicad_pcb >/dev/null 2>&1; done
kicad-cli pcb export pdf --layers F.Cu,F.SilkS,Edge.Cuts --mode-single -o ../fab/thermostat-v3-pcb-top.pdf thermostat-v3.kicad_pcb >/dev/null
kicad-cli pcb export pdf --layers B.Cu,B.SilkS,Edge.Cuts --mirror --mode-single -o ../fab/thermostat-v3-pcb-bottom.pdf thermostat-v3.kicad_pcb >/dev/null
for s in top bottom; do pdftoppm -png -r 250 -singlefile ../fab/thermostat-v3-pcb-$s.pdf ../fab/thermostat-v3-pcb-$s; done
kicad-cli pcb export pdf --layers F.Cu,B.Cu,F.SilkS,Edge.Cuts --mode-single -o /tmp/isl.pdf thermostat-v3.kicad_pcb >/dev/null
pdftoppm -png -r 600 -singlefile /tmp/isl.pdf /tmp/isl
kicad-cli sch export pdf -o ../fab/thermostat-v3-schematic.pdf thermostat-v3.kicad_sch >/dev/null
python3 - <<'P'
from PIL import Image
for s in('top','bottom'):
  im=Image.open('../fab/thermostat-v3-silk-%s.png'%s).convert('L'); bb=Image.eval(im,lambda v:255-v).getbbox(); im.crop(bb).save('../fab/thermostat-v3-silk-%s.png'%s)
im=Image.open('/tmp/isl.png'); W,H=im.size
# page is A4-ish; locate by board bbox of non-white
bb=Image.eval(im.convert('L'),lambda v:255-v).getbbox(); x0,y0,x1,y1=bb; sx=(x1-x0)/140; sy=(y1-y0)/64
im.crop((int(x0+114*sx),int(y0+20*sy),int(x0+139*sx),int(y0+44*sy))).save('../fab/thermostat-v3-island-zoom.png')
P
