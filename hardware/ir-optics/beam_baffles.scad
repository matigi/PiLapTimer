// PiLapTimer optical baffle prototypes.
// Check every mounting dimension against the actual enclosure before printing.
part = "both"; // "receiver", "beacon", or "both"

wall = 2.0;
flange_thickness = 2.5;
screw_diameter = 3.2;
screw_spacing_y = 18;
screw_spacing_z = 24;

receiver_depth = 35;
receiver_opening_y = 4;
receiver_opening_z = 10;

beacon_depth = 40;
beacon_opening_y = 3;
beacon_opening_z = 18;

$fn = 40;
epsilon = 0.1;

module flange(opening_y, opening_z, outer_y, outer_z) {
  difference() {
    cube([flange_thickness, outer_y, outer_z], center = false);
    translate([-epsilon, (outer_y-opening_y)/2, (outer_z-opening_z)/2])
      cube([flange_thickness + 2*epsilon, opening_y, opening_z], center = false);
    for (y = [-1, 1], z = [-1, 1])
      translate([-epsilon,
                 outer_y/2 + y*screw_spacing_y/2,
                 outer_z/2 + z*screw_spacing_z/2])
        rotate([0, 90, 0])
          cylinder(h = flange_thickness + 2*epsilon, d = screw_diameter);
  }
}

module rectangular_tunnel(depth, opening_y, opening_z) {
  outer_y = opening_y + 2*wall;
  outer_z = opening_z + 2*wall;
  difference() {
    cube([depth, outer_y, outer_z], center = false);
    translate([-epsilon, wall, wall])
      cube([depth + 2*epsilon, opening_y, opening_z], center = false);
  }
}

module baffle(depth, opening_y, opening_z, flange_y, flange_z) {
  flange(opening_y, opening_z, flange_y, flange_z);
  translate([flange_thickness,
             (flange_y - opening_y - 2*wall)/2,
             (flange_z - opening_z - 2*wall)/2])
    rectangular_tunnel(depth, opening_y, opening_z);
}

module receiver_baffle() {
  baffle(receiver_depth, receiver_opening_y, receiver_opening_z, 26, 32);
}

module beacon_baffle() {
  baffle(beacon_depth, beacon_opening_y, beacon_opening_z, 30, 38);
}

if (part == "receiver") {
  receiver_baffle();
} else if (part == "beacon") {
  beacon_baffle();
} else {
  receiver_baffle();
  translate([0, 38, 0]) beacon_baffle();
}
