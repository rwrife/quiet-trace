// Quiet Trace revision-A serviceable desktop enclosure
// Editable OpenSCAD source. Dimensions are millimetres.
// Static geometry only: acoustic and RF performance require prototype measurement.

$fn = 48;
part = "assembled"; // "base", "lid", or "assembled"

board = [80, 52, 1.6];
wall = 2.0;
clearance = 2.0;
outer = [board.x + 2 * (wall + clearance), board.y + 2 * (wall + clearance), 24];
base_height = 12;
lid_height = outer.z - base_height;
board_origin = [wall + clearance, wall + clearance, 3.0 + 3.0];

// Coordinates match hardware/kicad/quiet-trace.kicad_pcb.
mount_holes = [
    [3.475, 3.475],
    [76.525, 3.475],
    [76.525, 48.525],
    [3.475, 48.525]
];
mic_port = [40.0, 45.21];
usb_center_y = 24.0;
reset_center = [60.0, 32.0];
setup_center = [68.0, 32.0];
status_center = [69.0, 42.0];

module rounded_box(size, radius = 2) {
    minkowski() {
        cube([size.x - 2 * radius, size.y - 2 * radius, size.z - 2 * radius], center = false);
        translate([radius, radius, radius]) sphere(r = radius);
    }
}

module pcb_standoff(point) {
    translate([board_origin.x + point.x, board_origin.y + point.y, 3.0])
        difference() {
            cylinder(h = 3.0, d = 6.5);
            translate([0, 0, -0.1]) cylinder(h = 3.2, d = 3.2);
        }
}

module base() {
    difference() {
        rounded_box([outer.x, outer.y, base_height], 2.0);
        // Main cavity leaves a 3 mm floor and 2 mm walls.
        translate([wall, wall, 3.0])
            cube([outer.x - 2 * wall, outer.y - 2 * wall, base_height]);
        // USB-C side opening. Connector is accessible without bending the cable.
        translate([-0.1, board_origin.y + usb_center_y - 6.0, 6.8])
            cube([wall + 0.2, 12.0, 5.4]);
        // Straight acoustic opening below the PCB's 0.5 mm microphone sound hole.
        // The 3 mm enclosure port is intentionally conservative and uncharacterized.
        translate([board_origin.x + mic_port.x, board_origin.y + mic_port.y, -0.1])
            cylinder(h = 3.2, d = 3.0);
        // Four through-holes permit ordinary M3 fasteners and disassembly.
        for (point = mount_holes)
            translate([board_origin.x + point.x, board_origin.y + point.y, -0.1])
                cylinder(h = 3.2, d = 3.2);
    }
    for (point = mount_holes) pcb_standoff(point);
}

module lid() {
    translate([0, 0, base_height])
        difference() {
            rounded_box([outer.x, outer.y, lid_height], 2.0);
            // Open underside; 2 mm top skin.
            translate([wall, wall, -0.1])
                cube([outer.x - 2 * wall, outer.y - 2 * wall, lid_height - 2.0]);
            // Top access for reset and setup/mark buttons.
            for (point = [reset_center, setup_center])
                translate([board_origin.x + point.x, board_origin.y + point.y, lid_height - 2.1])
                    cylinder(h = 2.3, d = 5.0);
            // Status LED window; firmware must also communicate with timing/pattern.
            translate([board_origin.x + status_center.x, board_origin.y + status_center.y, lid_height - 2.1])
                cylinder(h = 2.3, d = 3.0);
            // Ventilation slots on the USB/power side, away from the antenna and mic port.
            for (y = [18, 23, 28, 33])
                translate([board_origin.x + 8, board_origin.y + y, lid_height - 2.1])
                    cube([16, 1.8, 2.3]);
            // Lid fastener clearance aligned with PCB/base mounting coordinates.
            for (point = mount_holes)
                translate([board_origin.x + point.x, board_origin.y + point.y, -0.1])
                    cylinder(h = lid_height + 0.2, d = 3.4);
        }
}

module pcb_envelope() {
    color([0.1, 0.45, 0.15, 0.55])
        translate(board_origin) cube(board);
}

if (part == "base") {
    base();
} else if (part == "lid") {
    lid();
} else {
    base();
    color([0.75, 0.78, 0.82, 0.35]) lid();
    pcb_envelope();
}
