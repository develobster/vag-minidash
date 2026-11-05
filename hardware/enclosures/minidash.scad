// MiniDash fit-test enclosures, 2026-11-05. MIT.
// Dimensions are DESIGN ALLOWANCES, not measurements of the user's boards.
// Export base and bezel separately. mm throughout. See README.md before printing.
device = "s3";      // s3, cyd, lite
variant = "compact"; // compact, hood, panel
part = "assembly"; // base, bezel, assembly, coupon
$fn = 40;

// Override these after caliper measurements. No PCB hole pattern is presumed.
body_w = device == "s3" ? 112 : device == "cyd" ? 102 : 108;
body_h = device == "s3" ? 86 : device == "cyd" ? 66 : 46;
body_d = device == "s3" ? 42 : device == "cyd" ? 32 : 24;
window_w = device == "s3" ? 50 : device == "cyd" ? 59 : 60;
window_h = device == "s3" ? 38 : device == "cyd" ? 45 : 20;
wall = 2.4;
floor_t = 2.4;
bezel_t = 2.4;
corner_r = 5;
fit = 0.35; // clearance per side, tune using coupon
screw_clearance = 3.4;
pilot_d = 2.6; // sample pilot for plastic-forming screw; verify actual hardware
post_od = 8;
post_inset = 8;
cable_d = 7; // placeholder for measured grommet, rear entry
hood_depth = 12;
flange = 8;

assert(body_w>window_w+2*wall && body_h>window_h+2*wall);
assert(body_d>floor_t+8 && fit>0 && wall>=2);

module rounded(w,h,r,z) {
    linear_extrude(height=z)
        offset(r=r) square([w-2*r,h-2*r],center=true);
}
module bosses() {
    for(x=[-1,1],y=[-1,1])
        translate([x*(body_w/2-post_inset),y*(body_h/2-post_inset),floor_t-0.1])
            difference() {
                cylinder(d=post_od,h=body_d-floor_t+0.1);
                translate([0,0,2]) cylinder(d=pilot_d,h=body_d);
            }
}
module screw_holes(h) {
    for(x=[-1,1],y=[-1,1])
        translate([x*(body_w/2-post_inset),y*(body_h/2-post_inset),-1])
            cylinder(d=screw_clearance,h=h+2);
}
module base() {
    difference() {
        union() {
            difference() {
                rounded(body_w,body_h,corner_r,body_d);
                translate([0,0,floor_t])
                    rounded(body_w-2*wall,body_h-2*wall,corner_r-wall,body_d+1);
            }
            bosses();
            // Four short internal ledges for a measured carrier, not PCB screws.
            for(x=[-1,1],y=[-1,1])
                translate([x*(body_w/2-wall-1),y*body_h/4,floor_t+3])
                    cube([3,12,3],center=true);
        }
        // Rear cable entry, intentionally no guessed USB/switch openings.
        translate([0,0,-1]) cylinder(d=cable_d,h=floor_t+2);
        // Tie-down pairs in rear floor; use insulated carrier, no bare PCB on ties.
        for(x=[-body_w/4,body_w/4],y=[-6,6])
            translate([x,y,floor_t/2]) cube([8,2.5,floor_t+2],center=true);
    }
}
module bezel() {
    difference() {
        union() {
            rounded(body_w+(variant=="panel"?2*flange:0),
                    body_h+(variant=="panel"?2*flange:0),corner_r,bezel_t);
            // Rear locating rim goes inside the shell. Export faces down on bed.
            translate([0,0,bezel_t-0.1]) difference() {
                rounded(body_w-2*wall-2*fit,body_h-2*wall-2*fit,2,2.1);
                translate([0,0,-1]) rounded(body_w-2*wall-2*fit-3,
                                           body_h-2*wall-2*fit-3,1,5);
                // Clear all four shell bosses.
                for(x=[-1,1],y=[-1,1])
                    translate([x*(body_w/2-post_inset),y*(body_h/2-post_inset),-1])
                        cylinder(d=post_od+2*fit,h=6);
            }
            if(variant=="hood") {
                translate([-body_w/2,body_h/2-wall,-hood_depth]) cube([body_w,wall,hood_depth+bezel_t]);
                for(x=[-1,1]) translate([x*(body_w/2-wall/2),body_h/2-10,-hood_depth])
                    linear_extrude(height=hood_depth+bezel_t) square([wall,20],center=true);
            }
        }
        translate([-window_w/2,-window_h/2,-1]) cube([window_w,window_h,hood_depth+8]);
        screw_holes(hood_depth+8);
        if(variant=="panel")
            for(x=[-1,1],y=[-1,1])
                translate([x*(body_w/2+flange/2),y*(body_h/2+flange/2),-1])
                    cylinder(d=screw_clearance,h=bezel_t+2);
    }
}
module coupon() {
    // Two separate pieces: a hole/slot test strip and mating square.
    difference() {
        cube([40,20,3]);
        for(i=[0:3]) translate([6+i*8,6,-1]) cylinder(d=2.4+i*0.2,h=5);
        translate([4,11,-1]) cube([10+2*fit,5+2*fit,5]);
    }
    translate([0,25,0]) cube([10,5,3]);
}
if(part=="base") base();
else if(part=="bezel") translate([0,0,variant=="hood"?hood_depth:0]) bezel();
else if(part=="coupon") coupon();
else {
    color("#30343a") base();
    // Bezel rim points down into the open base.
    color("#4e5968") translate([0,0,body_d+bezel_t]) rotate([0,180,0]) bezel();
}
