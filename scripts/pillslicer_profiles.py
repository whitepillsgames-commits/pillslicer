"""Generate the PillSlicer process profiles for the Kobra 3 and Kobra S1 (0.4 nozzle).

Each profile inherits the vendor's 0.20mm Standard profile for that machine and
overrides only what PillSlicer changes, so vendor fixes keep flowing through.
"""
import json
import sys
from pathlib import Path

PROCESS_DIR = Path(sys.argv[1])

# Shared by every PillSlicer profile: visual quality and easy-release supports.
COMMON = {
    # Walls & seams
    "wall_generator": "arachne",
    "wall_sequence": "inner wall/outer wall",
    "precise_outer_wall": "1",
    "seam_position": "aligned",
    "seam_slope_type": "external",       # scarf seam on the visible wall
    "seam_slope_conditional": "1",       # only where it actually helps
    "seam_slope_entire_loop": "0",
    "seam_slope_inner_walls": "0",
    "seam_slope_min_length": "10",
    "seam_slope_start_height": "0",
    "seam_slope_steps": "10",
    "scarf_joint_speed": "50%",
    "scarf_joint_flow_ratio": "1",
    "seam_gap": "10%",
    "small_perimeter_speed": "50%",
    "detect_thin_wall": "0",             # Arachne already handles thin features
    # Surfaces
    "top_surface_pattern": "monotonicline",
    "bottom_surface_pattern": "monotonic",
    "internal_solid_infill_pattern": "monotonic",
    "only_one_wall_top": "1",
    "top_bottom_infill_wall_overlap": "15%",
    # Overhangs & bridges
    "enable_overhang_speed": "1",
    "overhang_1_4_speed": "0",
    "overhang_2_4_speed": "50",
    "overhang_3_4_speed": "25",
    "overhang_4_4_speed": "10",
    "bridge_speed": "30",
    "bridge_flow": "0.9",
    "thick_bridges": "0",
    # Klipper firmware: arcs bring no benefit and can cause stutter
    "enable_arc_fitting": "0",
    # Supports: organic trees, clean underside, easy to snap off
    "support_type": "tree(auto)",
    "support_style": "organic",
    "support_threshold_angle": "30",
    "support_on_build_plate_only": "0",
    "support_remove_small_overhang": "1",
    "support_object_xy_distance": "0.4",
    # Economical supports: two interface layers (three on Detail and Quality, see STYLES).
    "support_interface_top_layers": "2",
    "support_interface_bottom_layers": "0",
    "support_interface_pattern": "rectilinear_interlaced",
    "support_interface_spacing": "0.25",
    "support_interface_not_for_body": "1",
    "support_interface_speed": "80",
    "support_line_width": "0.42",
    "support_base_pattern": "default",
    "support_base_pattern_spacing": "2.5",
    "independent_support_layer_height": "1",
    # Branches may lean up to 45 deg: measured to save filament on every test part (-28%, -8%, 0%).
    "tree_support_branch_angle_organic": "45",
    "tree_support_branch_diameter_organic": "2",
    "tree_support_branch_distance_organic": "1",
    "tree_support_tip_diameter": "0.8",
    "tree_support_auto_brim": "1",
    # Multicolour: no prime tower on layers without a filament change, so the tower stops climbing
    # the whole part when colours only change on some layers.
    "wipe_tower_no_sparse_layers": "1",
}

# What each profile changes, independent of the machine.
STYLES = {
    "Detail": {
        "layer": 0.12,
        "label": "fine details, figurines, text",
        "overrides": {
            "support_interface_top_layers": "3",
            "support_interface_speed": "60",
            "wall_loops": "3",
            "top_shell_layers": "7",
            "bottom_shell_layers": "5",
            "sparse_infill_density": "15%",
            "reduce_crossing_wall": "1",
            # Ironing the top support interface leaves a smoother underside on the part.
            "support_ironing": "1",
        },
    },
    "Quality": {
        "layer": 0.16,
        "label": "best visual result for display parts",
        "overrides": {
            "support_interface_top_layers": "3",
            "support_interface_speed": "60",
            "wall_loops": "3",
            "top_shell_layers": "6",
            "bottom_shell_layers": "4",
            "sparse_infill_density": "15%",
            "reduce_crossing_wall": "1",
            # Ironing the top support interface leaves a smoother underside on the part.
            "support_ironing": "1",
        },
    },
    "Balanced": {
        "layer": 0.20,
        "label": "everyday prints, good look at good speed",
        "overrides": {
            "wall_loops": "2",
            "top_shell_layers": "5",
            "bottom_shell_layers": "3",
            "sparse_infill_density": "15%",
        },
    },
    "Fast": {
        "layer": 0.24,
        "label": "drafts and quick functional parts",
        "overrides": {
            "wall_loops": "2",
            "top_shell_layers": "4",
            "bottom_shell_layers": "3",
            "sparse_infill_density": "12%",
            "inner_wall_line_width": "0.45",
            "sparse_infill_line_width": "0.5",
            "seam_slope_type": "none",
        },
    },
    "Strong": {
        "layer": 0.20,
        "label": "mechanical and load-bearing parts",
        "overrides": {
            "wall_loops": "4",
            "top_shell_layers": "6",
            "bottom_shell_layers": "5",
            "sparse_infill_density": "30%",
            "sparse_infill_pattern": "gyroid",
            "infill_wall_overlap": "25%",
        },
    },
}

# Machine-specific motion, tuned to the mechanics of each printer.
# Speeds in mm/s, accelerations in mm/s². The filament's max volumetric speed
# still caps everything, so these are ceilings, not guarantees.
MACHINES = {
    # Bed slinger: the moving bed rings at high accel, so outer walls stay gentle.
    "Anycubic Kobra 3 0.4 nozzle": {
        "base": "0.20mm Standard @Anycubic Kobra 3 0.4 nozzle",
        "motion": {
            "Detail":   dict(outer=70,  inner=150, solid=120, top=80,  sparse=200, oacc=1500, iacc=3000, tacc=1500, dacc=4000),
            "Quality":  dict(outer=90,  inner=180, solid=150, top=100, sparse=250, oacc=2000, iacc=4000, tacc=1500, dacc=5000),
            "Balanced": dict(outer=130, inner=200, solid=180, top=130, sparse=300, oacc=3000, iacc=5000, tacc=2000, dacc=6000),
            "Fast":     dict(outer=160, inner=250, solid=220, top=150, sparse=300, oacc=4000, iacc=6000, tacc=2500, dacc=8000),
            "Strong":   dict(outer=110, inner=180, solid=160, top=120, sparse=250, oacc=2500, iacc=5000, tacc=2000, dacc=6000),
        },
        "travel": dict(speed=350, acc=10000),
        # Support looks do not matter; the bed still carries the part, so stay below the S1.
        "support_speed": 180,
    },
    # CoreXY with a light toolhead: higher speeds, still soft on the visible wall.
    "Anycubic Kobra S1 0.4 nozzle": {
        "base": "0.20mm Standard @Anycubic Kobra S1 0.4 nozzle",
        "motion": {
            "Detail":   dict(outer=90,  inner=200, solid=180, top=100, sparse=250, oacc=2500, iacc=4000, tacc=2000, dacc=5000),
            "Quality":  dict(outer=120, inner=250, solid=220, top=150, sparse=270, oacc=3000, iacc=5000, tacc=2000, dacc=6000),
            "Balanced": dict(outer=170, inner=300, solid=250, top=180, sparse=300, oacc=4000, iacc=6000, tacc=2500, dacc=8000),
            "Fast":     dict(outer=220, inner=330, solid=280, top=200, sparse=350, oacc=5000, iacc=8000, tacc=3000, dacc=10000),
            "Strong":   dict(outer=150, inner=250, solid=220, top=160, sparse=270, oacc=3500, iacc=6000, tacc=2500, dacc=8000),
        },
        "travel": dict(speed=400, acc=10000),
        "support_speed": 250,
    },
}


def fmt(x):
    return f"{x:g}"


def build(machine, cfg, style_name, style):
    layer = style["layer"]
    m = cfg["motion"][style_name]
    name = f"{layer:.2f}mm {style_name} PillSlicer @{machine}"
    # Support gap: one layer, never below 0.15 mm so supports still release.
    gap = max(0.15, round(layer, 2))
    prof = {
        "type": "process",
        "name": name,
        "inherits": cfg["base"],
        "from": "system",
        "instantiation": "true",
        "print_settings_id": name,
        "layer_height": fmt(layer),
        "compatible_printers": [machine],
        "notes": f"PillSlicer — {style['label']}.",
    }
    prof.update(COMMON)
    prof.update({
        "outer_wall_speed": fmt(m["outer"]),
        "inner_wall_speed": fmt(m["inner"]),
        "internal_solid_infill_speed": fmt(m["solid"]),
        "top_surface_speed": fmt(m["top"]),
        "sparse_infill_speed": fmt(m["sparse"]),
        "gap_infill_speed": fmt(m["outer"]),
        "support_speed": fmt(cfg["support_speed"]),
        "outer_wall_acceleration": fmt(m["oacc"]),
        "inner_wall_acceleration": fmt(m["iacc"]),
        "top_surface_acceleration": fmt(m["tacc"]),
        "default_acceleration": fmt(m["dacc"]),
        "internal_solid_infill_acceleration": fmt(m["dacc"]),
        "sparse_infill_acceleration": fmt(m["dacc"]),
        "bridge_acceleration": "50%",
        "travel_speed": fmt(cfg["travel"]["speed"]),
        "travel_acceleration": fmt(cfg["travel"]["acc"]),
        "support_top_z_distance": fmt(gap),
        "support_bottom_z_distance": fmt(gap),
    })
    prof.update(style["overrides"])
    return name, prof


def main():
    written = []
    for machine, cfg in MACHINES.items():
        for style_name, style in STYLES.items():
            name, prof = build(machine, cfg, style_name, style)
            path = PROCESS_DIR / f"{name}.json"
            path.write_text(json.dumps(prof, indent="\t", ensure_ascii=False) + "\n", encoding="utf-8")
            written.append(name)
    print("\n".join(written))


if __name__ == "__main__":
    main()
