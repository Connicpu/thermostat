cd /workspace/thermostat/v3
python3 gen_pcb2.py kicad/thermostat-v3.kicad_pcb 2>&1 | grep -v assert
cp pro_template.json kicad/thermostat-v3.kicad_pro; python3 export_dsn.py route.dsn 2>&1 | grep -v assert; python3 fix_dsn.py route.dsn; cp pro_template.json kicad/thermostat-v3.kicad_pro
timeout 300 xvfb-run -a java -jar /workspace/freerouting.jar -de route.dsn -do route.ses -mp ${MP:-40} > /tmp/fr.log 2>&1
grep -iE 'completed' /tmp/fr.log | tail -1
cd kicad && python3 ../finish_pcb.py 2>&1 | grep -v assert; python3 ../clean_dangling.py 2>&1 | grep -v -e assert -e leak; cp ../pro_template.json thermostat-v3.kicad_pro
kicad-cli pcb drc --severity-all -o drc.rpt thermostat-v3.kicad_pcb >/dev/null; grep '^\[' drc.rpt | sed 's/\]:.*/]/' | sort | uniq -c
cd /workspace/thermostat/v3/kicad && kicad-cli pcb drc --schematic-parity -o drc.rpt thermostat-v3.kicad_pcb >/dev/null; python3 ../silk_fix.py 2>&1 | grep -v -e assert -e leak -e shape_poly; cp ../pro_template.json thermostat-v3.kicad_pro
