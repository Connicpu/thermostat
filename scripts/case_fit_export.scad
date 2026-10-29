include <thermostat_case.scad>
if(which=="b") backplate();
else if(which=="c") carrier();
else if(which=="r") retainer();
else if(which=="g") wire_plug();
else if(which=="k") cap_placed(btn_flare, 0);          // seated: cone in the hole, spring on the lip
else if(which=="kp") cap_placed(btn_flare, btn_travel); // pressed: barb on the lip
else if(which=="kr") cap_placed(0, 0);                 // print pose at the seat, barbs sprung out
else shell(false);
