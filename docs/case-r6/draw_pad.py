#!/usr/bin/env python3
"""Section and plan for the revised pad retainer."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch

OUT = os.path.dirname(os.path.abspath(__file__))
CAR, PAD, PLATE = "#c4a574", "#d7b56d", "#e7d7ad"
PANEL, SHELL, EDGE = "#f4f4f4", "#6d8ea8", "#222"
LOAD, NOTE = "#9b2335", "#1f4e79"


def box(ax, x, y, w, h, fc, lw=0.8):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor=EDGE, lw=lw, zorder=2))


def section(path):
    fig, ax = plt.subplots(figsize=(12.8, 6.4), dpi=140)
    # y outboard-left, z up. Millimetres, local.
    box(ax, -3.6, 20.6, 1.44, 5.7, SHELL)             # wall, inner face -2.16
    box(ax, -2.16, 25.88, 4.3, 0.37, SHELL)           # pocket over the cap
    box(ax, 2.15, 25.51, 1.6, 0.74, SHELL)            # original ceiling over the land
    box(ax, 0.0, 23.10, 4.4, 1.20, PANEL)
    box(ax, 1.00, 19.55, 2.30, 3.00, PAD)             # beam
    box(ax, 1.90, 22.55, 1.20, 0.53, PAD)             # seated nub
    box(ax, -0.5, 19.55, 1.6, 3.00, PAD)              # root
    box(ax, -0.45, 22.57, 0.40, 1.20, CAR)            # clamp bar
    box(ax, -1.05, 23.90, 0.90, 0.60, PLATE)          # outboard bar
    box(ax, 0.0, 24.50, 2.05, 0.60, PLATE)            # web, clear of the glass
    box(ax, 2.00, 24.36, 1.20, 0.40, PLATE)           # land
    box(ax, -1.90, 24.56, 1.70, 0.60, CAR)            # cap
    box(ax, 0.35, 21.55, 1.20, 1.20, CAR)             # backstop, drawn in section at the edge
    ax.plot([3.32, 3.32], [24.2, 25.51], color=SHELL, lw=0.8, ls="--")
    ax.annotate("", xy=(2.6, 22.7), xytext=(2.6, 23.05),
                arrowprops=dict(arrowstyle="-|>", color=LOAD, lw=1.1))
    ax.text(2.75, 22.85, "pad", color=LOAD, fontsize=8)
    ax.annotate("", xy=(2.6, 25.05), xytext=(2.6, 24.55),
                arrowprops=dict(arrowstyle="-|>", color=LOAD, lw=1.1))
    ax.text(2.75, 24.85, "land", color=LOAD, fontsize=8)
    ax.text(3.4, 24.7, "window", color=SHELL, fontsize=7)
    ax.text(-3.3, 26.05, "shell", color=SHELL, fontsize=8)
    ax.text(0.2, 20.2, "0.95 mm air under the beam", color=NOTE, fontsize=8)
    ax.set_xlim(-3.8, 5.2)
    ax.set_ylim(19.2, 26.5)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Section at a pad   y outboard to the left, z up", loc="left", fontsize=12)
    fig.text(0.01, 0.01,
             "Land y 2.00–3.20 is the only contact. Beam 18 × 2.4 × 3.00. "
             "Cap 0.60 mm. Backstop top 22.75. Nub top, seated, 23.08.",
             fontsize=8, color="#333")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def plan(path):
    fig, ax = plt.subplots(figsize=(13.4, 6.6), dpi=140)
    box(ax, 29.46, 26.24, 79.0, 36.70, PANEL, lw=1.0)
    box(ax, 32.46, 30.06, 66.9, 29.06, "#e6e6e6", lw=0.5)
    # trouble parts, drawn light so the clearance is visible
    box(ax, 28.8, 21.3, 11.4, 7.8, "#f6d0d0", lw=0.4)     # U7
    box(ax, 66.05, 0.0, 18.0, 25.5, "#f6d0d0", lw=0.4)    # U3
    box(ax, 42.9, 54.25, 3.2, 6.0, "#f6d0d0", lw=0.4)     # C1
    box(ax, 49.5, 56.5, 29.5, 3.0, "#f6d0d0", lw=0.4)     # tails
    ax.text(31, 24.5, "U7", color="#944", fontsize=7)
    ax.text(70, 12, "U3", color="#944", fontsize=7)
    ax.text(55, 58.2, "tails", color="#944", fontsize=7)
    stations = ((64, 26.24, 1), (100, 26.24, 1), (72, 62.94, -1), (100, 62.94, -1))
    for nub, edge, dire in stations:
        y0 = edge + dire * 2.00
        box(ax, nub - 16, min(y0, y0 + dire * 1.20), 22, 1.20, PLATE, lw=0.6)
        yc = edge + dire * 2.50
        box(ax, nub - 2, min(yc, yc - dire * 0.6), 4, 1.2, PAD, lw=0.5)
        ax.plot([nub, nub], [edge, edge + dire * 2.5], color=LOAD, lw=0.7, zorder=4)
    ax.annotate("nub and land\nsame centre", xy=(64, 28.7), xytext=(48, 18),
                fontsize=8, color=LOAD, arrowprops=dict(arrowstyle="->", color=LOAD, lw=0.7))
    ax.set_xlim(18, 118)
    ax.set_ylim(72, 8)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Plan   board x to the right, y down. Each plate is 22 mm.", loc="left", fontsize=12)
    fig.text(0.01, 0.01,
             "Nubs at x 64 and 100 on the top edge, x 72 and 100 on the bottom. "
             "Beams stay off U7, the ESP32 module, C1 and the J1 tails.",
             fontsize=8, color="#333")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    section(os.path.join(OUT, "concept-pad-section.png"))
    plan(os.path.join(OUT, "concept-pad-plan.png"))
