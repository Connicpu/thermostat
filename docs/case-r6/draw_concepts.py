#!/usr/bin/env python3
"""Dimensioned sections for the three retainer concepts. Not the case model."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Polygon, Rectangle, FancyBboxPatch

OUT = os.path.dirname(os.path.abspath(__file__))

CAR = "#c4a574"
CLIP = "#e6d3a3"
PANEL = "#f7f7f7"
EDGE = "#1f1f1f"
SHELL = "#8aa0b4"
LOAD = "#9b2335"
NOTE = "#1f4e79"


def _box(ax, x, y, w, h, fc, ec=EDGE, lw=1.1, z=2):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor=ec, lw=lw, zorder=z))


def _poly(ax, pts, fc, ec=EDGE, lw=1.1, z=2):
    ax.add_patch(Polygon(pts, closed=True, facecolor=fc, edgecolor=ec, lw=lw, zorder=z))


def _dim_h(ax, x0, x1, y, text, dy=0.18):
    ax.annotate("", xy=(x0, y), xytext=(x1, y),
                arrowprops=dict(arrowstyle="<->", color=NOTE, lw=0.8))
    ax.text((x0 + x1) / 2, y + dy, text, ha="center", va="bottom", color=NOTE, fontsize=8)


def _dim_v(ax, x, y0, y1, text, dx=0.15):
    ax.annotate("", xy=(x, y0), xytext=(x, y1),
                arrowprops=dict(arrowstyle="<->", color=NOTE, lw=0.8))
    ax.text(x + dx, (y0 + y1) / 2, text, ha="left", va="center", color=NOTE, fontsize=8, rotation=90)


def _arrow(ax, x, y, dx, dy, text, tx=None, ty=None):
    ax.add_patch(FancyArrowPatch((x, y), (x + dx, y + dy), arrowstyle="-|>",
                                 mutation_scale=12, color=LOAD, lw=1.4, zorder=5))
    ax.text(tx if tx is not None else x + dx + 0.08,
            ty if ty is not None else y + dy, text, color=LOAD, fontsize=8, va="center")


def _frame(ax, title, subtitle):
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title(title, loc="left", fontsize=13, pad=8, color="#111")
    ax.text(0.0, 1.02, subtitle, transform=ax.transAxes, fontsize=8, color="#333")


def concept_a(path):
    fig, ax = plt.subplots(figsize=(13.2, 7.4), dpi=140)
    # y to the right is inboard. z up. Numbers are millimetres, local to the section.
    # Fence and rib (carrier).
    _box(ax, 0.0, 0.0, 1.00, 3.55, CAR)          # fence
    _box(ax, -0.45, 0.70, 0.45, 0.60, CAR)       # rib body, square underside
    _poly(ax, [(-0.45, 1.30), (-0.15, 1.30), (-0.45, 1.00)], CAR)  # install chamfer, top-outer only
    # Clip.
    _box(ax, 0.0, 3.55, 1.00, 0.80, CLIP)        # root, bearing on the fence, 0 gap
    _poly(ax, [(1.00, 3.55), (1.35, 3.95), (1.35, 4.35), (1.00, 4.35)], CLIP)  # 45 root chamfer
    _box(ax, 1.35, 3.95, 3.20, 0.40, CLIP)       # tongue plate
    _box(ax, 3.70, 3.75, 0.40, 0.20, CLIP)       # bump, 0.20 below the plate
    # Tooth drawn in the free-end section: square top, lead-in on the bottom-inner corner.
    _box(ax, -0.80, 0.70, 0.50, 0.60, CLIP)      # arm section (outboard)
    _poly(ax, [(-0.30, 0.70), (-0.05, 0.70), (-0.05, 1.00), (-0.30, 1.30)], CLIP)
    # Panel and shell.
    _box(ax, 1.20, 3.55, 4.6, 0.18, PANEL, lw=0.8)
    ax.plot([-1.55, -1.55], [-0.2, 5.2], color=SHELL, lw=1.2, ls="--")
    ax.text(-1.52, 5.05, "shell\n(air only)", color=SHELL, fontsize=8, va="top")
    # Load path.
    _arrow(ax, 3.90, 3.55, 0, -0.55, "preload into the panel", tx=4.05, ty=2.85)
    _arrow(ax, 0.55, 3.55, 0, -0.45, "root bears on the fence", tx=1.15, ty=2.95)
    _arrow(ax, -0.18, 1.30, 0, 0.45, "square hook, 0 z gap", tx=0.15, ty=1.95)
    # Dimensions.
    _dim_h(ax, -0.45, 0.0, 0.45, "0.45 proud")
    _dim_h(ax, -0.30, -0.05, 1.55, "0.35 bite")
    _dim_v(ax, -0.95, 0.70, 1.30, "0 gap", dx=-0.55)
    _dim_h(ax, 1.35, 3.90, 4.55, "a = 2.95 to bump centre")
    _dim_v(ax, 4.30, 3.75, 3.95, "0.20", dx=0.08)
    _dim_h(ax, -1.55, -0.80, 2.40, "≥ 0.21 air")
    ax.annotate("install lead-in\nonly here", xy=(-0.45, 1.15), xytext=(-2.3, 1.7),
                fontsize=8, color=NOTE, arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7))
    ax.annotate("retention face\nis square", xy=(-0.20, 1.30), xytext=(0.7, 1.55),
                fontsize=8, color=NOTE, arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7))
    # Print and plan insets.
    ax.text(5.15, 4.7, "Print (clip)\nbump face on the bed\nlayers stack in +z\narm thickness is in y\nso the snap is in-plane\npillars 0.80 if a bridge remains",
            fontsize=8, va="top", family="monospace",
            bbox=dict(boxstyle="round", fc="#f4f7fb", ec=NOTE, lw=0.6))
    ax.text(5.15, 2.15, "Plan\nanchor |—— arm ——| tooth\ntooth is the last 1.6 mm\nnot the whole arm",
            fontsize=8, va="top", family="monospace",
            bbox=dict(boxstyle="round", fc="#f4f7fb", ec=NOTE, lw=0.6))
    ax.text(0.0, -0.85, "Section through the free end. Inboard is to the right. Carrier is brown, clip is tan, panel is the thin bar.\n"
            "The shell line is outboard and is not a stop. Install and removal happen with the shell off.",
            fontsize=8, color="#333")
    ax.set_xlim(-2.6, 9.6)
    ax.set_ylim(-1.15, 5.5)
    _frame(ax, "Concept A — square hook, root on the fence",
           "Leading concept. Carrier feet-down. Clip prints bump-down, layers along case z. Load path stays in the carrier.")
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def concept_b(path):
    fig, ax = plt.subplots(figsize=(13.2, 7.4), dpi=140)
    _box(ax, 0.0, 0.0, 1.00, 3.55, CAR)
    # Undercut rib: underside slopes so the outer lip is the low point.
    _poly(ax, [(-0.50, 0.62), (-0.05, 0.78), (-0.05, 1.30), (-0.50, 1.30)], CAR)
    _poly(ax, [(-0.50, 1.30), (-0.20, 1.30), (-0.50, 1.00)], CAR)
    _box(ax, 0.0, 3.55, 1.00, 0.80, CLIP)
    _poly(ax, [(1.00, 3.55), (1.35, 3.95), (1.35, 4.35), (1.00, 4.35)], CLIP)
    _box(ax, 1.35, 3.95, 3.20, 0.40, CLIP)
    _box(ax, 3.70, 3.75, 0.40, 0.20, CLIP)
    # Matching undercut tooth.
    _box(ax, -0.85, 0.55, 0.45, 0.70, CLIP)
    _poly(ax, [(-0.40, 0.62), (-0.05, 0.78), (-0.05, 1.05), (-0.28, 1.30), (-0.40, 1.15)], CLIP)
    _box(ax, 1.20, 3.55, 4.6, 0.18, PANEL, lw=0.8)
    ax.plot([-1.70, -1.70], [-0.2, 5.2], color=SHELL, lw=1.2, ls="--")
    ax.text(-1.67, 5.05, "shell\n(air only)", color=SHELL, fontsize=8, va="top")
    _arrow(ax, 3.90, 3.55, 0, -0.55, "same preload path", tx=4.05, ty=2.85)
    _arrow(ax, -0.15, 0.78, -0.35, 0.15, "undercut pulls inboard", tx=-2.15, ty=1.15)
    _dim_h(ax, -0.50, -0.05, 0.35, "8° undercut")
    _dim_v(ax, 4.30, 3.75, 3.95, "0.20", dx=0.08)
    _dim_h(ax, 1.35, 3.90, 4.55, "same tongue, a = 2.95")
    ax.text(5.15, 4.7, "Same print as A\ntooth on the bed\nlayers in +z\nundercut is a horizontal\nlip, self-supporting\non the carrier",
            fontsize=8, va="top", family="monospace",
            bbox=dict(boxstyle="round", fc="#f4f7fb", ec=NOTE, lw=0.6))
    ax.text(0.0, -0.85, "The hook reaction has an inboard component, away from the shell.\n"
            "Install deflection is 0.41 mm instead of 0.35 mm. The 6 mm span uses a 0.45 mm arm.",
            fontsize=8, color="#333")
    ax.set_xlim(-2.6, 9.6)
    ax.set_ylim(-1.15, 5.5)
    _frame(ax, "Concept B — 8° undercut hook",
           "Same root bearing and tongue as A. The retention face locks toward the fence.")
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def concept_c(path):
    fig, ax = plt.subplots(figsize=(13.2, 7.4), dpi=140)
    _box(ax, 0.0, 0.0, 2.4, 2.6, CAR)            # carrier boss
    _box(ax, 0.55, 1.3, 1.3, 0.7, "#f4e1c4")     # captive M2 nut
    # Leaf.
    _box(ax, 0.3, 2.6, 5.2, 0.55, CLIP)
    _box(ax, 4.6, 2.40, 0.50, 0.20, CLIP)        # bump
    # Screw.
    _box(ax, 0.95, 2.6, 0.50, 1.5, "#b9c0c7")
    _box(ax, 0.70, 3.85, 1.00, 0.45, "#b9c0c7")  # head
    _box(ax, 0.40, 3.70, 1.60, 0.15, CAR)        # shoulder stop the head lands on
    _box(ax, 2.2, 2.35, 3.6, 0.16, PANEL, lw=0.8)
    ax.plot([6.6, 6.6], [0.2, 4.8], color=SHELL, lw=1.2, ls="--")
    ax.text(6.65, 4.7, "shell\n(air only)", color=SHELL, fontsize=8, va="top")
    _arrow(ax, 4.85, 2.40, 0, -0.5, "preload into the panel", tx=5.0, ty=1.7)
    _arrow(ax, 1.2, 3.85, 0, -0.35, "head stops here", tx=1.85, ty=3.55)
    _dim_v(ax, 5.4, 2.40, 2.60, "δ 0.25 at 1.2 mm", dx=0.1)
    _dim_h(ax, 1.2, 4.85, 4.55, "leaf L = 5.5")
    _dim_v(ax, 6.35, 2.6, 3.3, "≥ 0.30 air", dx=0.08)
    ax.text(0.05, 5.15, "Print\nleaf flat on the bed\nlayers in +z\nbend is in-plane\nno pillars",
            fontsize=8, va="top", family="monospace",
            bbox=dict(boxstyle="round", fc="#f4f7fb", ec=NOTE, lw=0.6))
    ax.text(0.0, -0.7, "M2 screw into a trapped square nut. The head bottoms on the shoulder, so extra turns do not add deflection.\n"
            "The nut and the boss are in compression. The shell is outside the recess and is not in the load path.",
            fontsize=8, color="#333")
    ax.set_xlim(-0.3, 8.4)
    ax.set_ylim(-1.0, 5.6)
    _frame(ax, "Concept C — screw-stopped leaf",
           "Standard M2 screw. Preload is the leaf deflection when the head hits the stop.")
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    concept_a(os.path.join(OUT, "concept-a-square-hook.png"))
    concept_b(os.path.join(OUT, "concept-b-undercut.png"))
    concept_c(os.path.join(OUT, "concept-c-screw-bracket.png"))
    print("wrote three pngs")
