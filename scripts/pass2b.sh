cd /workspace/thermostat/v3/kicad
true || python3 -c "
import pcbnew; b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb')
zs=[z for z in b.Zones()]
rm=[z for z in zs if (not z.GetIsRuleArea() and z.GetNetname()=='GND') or (z.GetIsRuleArea() and not z.GetDoNotAllowTracks())]
for z in rm: b.Remove(z)
b.Save('thermostat-v3.kicad_pcb')" 2>&1 | grep -v -e assert -e leak
cd ..; cp pro_template.json kicad/thermostat-v3.kicad_pro; python3 export_dsn.py route2.dsn 2>&1 | grep -v assert; python3 fix_dsn.py route2.dsn >/dev/null
timeout 300 xvfb-run -a java -jar /workspace/freerouting.jar -de route2.dsn -do route.ses -mp 100 > /tmp/fr2.log 2>&1; grep -iE 'completed' /tmp/fr2.log | tail -1
cd kicad && python3 ../finish_pcb.py 2>&1 | grep -v assert; cp ../pro_template.json thermostat-v3.kicad_pro
kicad-cli pcb drc --severity-all -o drc.rpt thermostat-v3.kicad_pcb >/dev/null; grep '^\[' drc.rpt | sed 's/\]:.*/]/' | sort | uniq -c
