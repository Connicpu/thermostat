#!/usr/bin/env python3
"""Section and plan of the revised retainer. Not the case model."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Polygon, Rectangle, Circle

OUT = os.path.dirname(os.path.abspath(__file__))
CAR = "#c4a574"
CLIP = "#e7d7ad"
PANEL = "#f3f3f3"
EDGE = "#222"
SHELL = "#6d8ea8"
LOAD = "#9b2335"
NOTE = "#1f4e79"


def box(ax, x, y, w, h, fc, lw=1.05):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor=EDGE, lw=lw, zorder=2))


def arrow(ax, x, y, dx, dy, text, tx, ty, ha="left"):
    ax.add_patch(FancyArrowPatch((x, y), (x + dx, y + dy), arrowstyle="-|>",
                                 mutation_scale=11, color=LOAD, lw=1.3, zorder=5))
    ax.text(tx, ty, text, color=LOAD, fontsize=8, ha=ha, va="center")


def dim_h(ax, x0, x1, y, text):
    ax.annotate("", xy=(x0, y), xytext=(x1, y),
                arrowprops=dict(arrowstyle="<->", color=NOTE, lw=0.8))
    ax.text((x0 + x1) / 2, y + 0.12, text, ha="center", va="bottom", color=NOTE, fontsize=7.5)


def section(path):
    fig, ax = plt.subplots(figsize=(13.4, 6.6), dpi=140)
    # x to the right, z up. Millimetres, local.
    # Shelf x 0–1.0, lip x 1.2–2.8, free leaf from 2.8, bump centre 7.5.
    box(ax, -0.3, 0.0, 1.8, 3.20, CAR)             # shelf post
    box(ax, 1.2, 0.0, 1.6, 4.00, CAR)              # lip post
    box(ax, 1.2, 4.00, 1.6, 0.70, CAR)             # lip
    # 45° install lead-in on the lip's upstream lower corner only.
    ax.add_patch(Polygon([(1.2, 4.00), (1.55, 4.00), (1.2, 4.35)],
                         closed=True, facecolor="white", edgecolor=EDGE, lw=1.0, zorder=3))
    box(ax, 0.0, 3.20, 2.80, 0.80, CLIP)           # anchor plate
    box(ax, 2.80, 3.40, 4.70, 0.60, CLIP)          # leaf
    box(ax, 7.00, 3.00, 1.00, 0.40, CLIP)          # bump
    box(ax, 7.90, 3.40, 1.50, 0.45, CLIP)          # tab
    box(ax, 6.7, 2.78, 1.6, 0.14, PANEL, lw=0.7)
    ax.plot([9.7, 9.7], [0.2, 5.3], color=SHELL, lw=1.15, ls="--")
    ax.text(9.78, 5.15, "shell\nnot in the path", color=SHELL, fontsize=8, va="top")

    # Forces on the clip. R and F point up. H points down.
    arrow(ax, 0.50, 2.40, 0, 0.75, "R up, bearing", 0.15, 2.15, ha="center")
    arrow(ax, 2.00, 4.85, 0, -0.70, "H down, hold-down", 2.15, 5.05)
    arrow(ax, 7.50, 2.30, 0, 0.65, "F up, bump", 7.65, 2.05)
    ax.annotate("lip between\nshelf and bump", xy=(2.0, 4.35), xytext=(3.3, 5.15),
                fontsize=8, color=NOTE, arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7))
    dim_h(ax, 0.0, 1.0, 2.55, "shelf 1.0")
    dim_h(ax, 1.2, 2.8, 5.05, "lip 1.6")
    dim_h(ax, 2.8, 7.5, 4.55, "L = 4.70")
    dim_h(ax, 6.9, 8.1, 2.55, "bump 1.2")
    ax.plot([2.75, 2.75], [3.85, 4.00], color=NOTE, lw=1.2)
    ax.text(2.85, 3.72, "tooth\n0.05 x play", color=NOTE, fontsize=7)
    ax.text(0.05, -0.55,
            "Section along the leaf (x–z). Shelf pushes up, lip pushes down, bump pushes up.\n"
            "H = 3.67 F and R = 2.67 F. The latch tooth only stops the slide back out.",
            fontsize=8, color="#333")
    ax.set_xlim(-0.6, 10.6)
    ax.set_ylim(-1.15, 5.7)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Revised concept — closed preload loop", loc="left", fontsize=13, color="#111")
    ax.text(0, 1.03, "Carrier feet-down. Clip prints retention-face up. Layers stack in +z.",
            transform=ax.transAxes, fontsize=8, color="#333")
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def plan(path):
    fig, ax = plt.subplots(figsize=(13.4, 6.2), dpi=140)
    # x right, y up = inboard.
    box(ax, 0.0, 0.0, 2.80, 1.20, CLIP)            # plate
    box(ax, 2.80, 0.0, 5.20, 1.20, CLIP)           # leaf
    box(ax, 6.90, 0.0, 1.20, 1.20, "#f0d7a0")      # bump footprint
    box(ax, 7.90, 0.15, 1.70, 0.90, CLIP)          # tab
    box(ax, 1.20, -0.10, 1.60, 1.20, CAR)          # lip
    box(ax, -0.30, -0.20, 1.50, 1.40, CAR)         # shelf
    # stop peg inside the plate
    box(ax, 0.35, 0.28, 0.30, 0.54, "#8d6b45")
    ax.plot([-0.5, 10.2], [-0.30, -0.30], color=SHELL, lw=1.15, ls="--")
    ax.text(8.6, -0.48, "shell wall, 0.30 mm from the seated outer face", color=SHELL, fontsize=8)
    ax.annotate("outboard stop\nplay 0.08", xy=(0.50, 0.28), xytext=(-1.55, 1.55),
                fontsize=8, color=NOTE, arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7))
    ax.annotate("release tab", xy=(8.7, 0.5), xytext=(8.7, 1.7),
                fontsize=8, color=NOTE, arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7),
                ha="center")
    dim_h(ax, 0.0, 1.0, -1.05, "bearing")
    dim_h(ax, 1.2, 2.8, -1.05, "hold-down")
    dim_h(ax, 2.8, 7.5, 1.85, "leaf to bump centre 4.70")
    ax.text(7.5, 0.5, "F", color=LOAD, fontsize=11, ha="center", va="center", fontweight="bold")
    ax.text(2.0, 0.55, "H", color=LOAD, fontsize=11, ha="center", va="center", fontweight="bold")
    ax.text(0.5, 0.7, "R", color=LOAD, fontsize=11, ha="center", va="center", fontweight="bold")
    ax.text(-0.2, 2.35,
            "Plan. Inboard is up. R, H and F are on one line in y.\n"
            "The lip covers the anchor, so the hold-down is between the shelf and the bump in x.\n"
            "The stop peg is inside the plate. The tab stays inside the leaf's y.",
            fontsize=8, color="#333")
    ax.set_xlim(-1.8, 10.4)
    ax.set_ylim(-1.5, 2.9)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Revised concept — plan", loc="left", fontsize=13, color="#111")
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    section(os.path.join(OUT, "concept-rev-section.png"))
    plan(os.path.join(OUT, "concept-rev-plan.png"))
    print("ok")
