#!/usr/bin/env python3
"""Sections and plan for the under-panel pad retainer. Not the case model."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Rectangle, Polygon

OUT = os.path.dirname(os.path.abspath(__file__))
CAR = "#c4a574"
PAD = "#d7b56d"
PLATE = "#e7d7ad"
PANEL = "#f4f4f4"
EDGE = "#222"
SHELL = "#6d8ea8"
LOAD = "#9b2335"
NOTE = "#1f4e79"


def box(ax, x, y, w, h, fc, lw=0.9, z=2, ls="-"):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor=EDGE,
                           lw=lw, zorder=z, linestyle=ls))


def arrow(ax, x, y, dx, dy, text, tx, ty, ha="left", va="center"):
    ax.add_patch(FancyArrowPatch((x, y), (x + dx, y + dy), arrowstyle="-|>",
                                 mutation_scale=11, color=LOAD, lw=1.2, zorder=6))
    ax.text(tx, ty, text, color=LOAD, fontsize=8, ha=ha, va=va)


def dim(ax, x0, x1, y, text, above=True):
    ax.annotate("", xy=(x0, y), xytext=(x1, y),
                arrowprops=dict(arrowstyle="<->", color=NOTE, lw=0.7))
    dy = 0.08 if above else -0.08
    va = "bottom" if above else "top"
    ax.text((x0 + x1) / 2, y + dy, text, ha="center", va=va, color=NOTE, fontsize=7)


def dim_v(ax, x, y0, y1, text, right=True):
    ax.annotate("", xy=(x, y0), xytext=(x, y1),
                arrowprops=dict(arrowstyle="<->", color=NOTE, lw=0.7))
    dx = 0.08 if right else -0.08
    ha = "left" if right else "right"
    ax.text(x + dx, (y0 + y1) / 2, text, ha=ha, va="center", color=NOTE, fontsize=7)


def section(path):
    fig, (ax, bx) = plt.subplots(
        1, 2, figsize=(14.2, 6.8), dpi=140,
        gridspec_kw={"width_ratios": [1.15, 1.0]})

    # y-z through the nub. Outboard is left. z is case z.
    box(ax, -3.55, 21.2, 1.39, 5.05, SHELL)                 # wall, inner face y=-2.16
    box(ax, -2.16, 25.80, 2.36, 0.45, SHELL)                # pocket over the outboard washer
    box(ax, 0.20, 25.51, 3.12, 0.74, SHELL)                 # ceiling over the border
    ax.plot([3.32, 3.32], [24.2, 25.51], color=SHELL, lw=1.0, ls="--")
    box(ax, 0.0, 23.10, 4.3, 1.20, PANEL)                   # 1.2 panel, front at 24.30
    box(ax, -1.45, 21.2, 1.25, 3.10, CAR)                   # shoulder, outboard
    box(ax, -1.60, 24.30, 4.05, 0.40, PLATE)                # plate onto the land
    box(ax, -1.50, 24.70, 1.20, 0.20, CAR)                  # washer on the outboard plate
    box(ax, -2.05, 24.70, 0.55, 0.20, CAR)                  # ear
    box(ax, 0.60, 21.05, 3.30, 1.40, PAD)                   # beam width
    box(ax, 1.85, 22.45, 0.80, 0.65, PAD)                   # nub up to the 1.2 back
    ax.plot([0.65, 3.85], [23.20, 23.20], color=NOTE, lw=0.7, ls=":")
    ax.plot([0.65, 3.85], [23.00, 23.00], color=NOTE, lw=0.7, ls=":")
    ax.text(3.95, 23.20, "1.1 back", color=NOTE, fontsize=7, va="center")
    ax.text(3.95, 23.00, "1.3 back", color=NOTE, fontsize=7, va="center")

    arrow(ax, 2.25, 22.55, 0, 0.42, "pad", 2.55, 22.7)
    arrow(ax, 2.25, 25.20, 0, -0.38, "plate", 2.55, 25.15, va="bottom")
    ax.plot([1.52, 1.52], [23.10, 24.30], color=LOAD, lw=0.6, ls=":")
    ax.text(1.52, 22.85, "PS", color=LOAD, fontsize=7, ha="center")
    ax.text(3.32, 24.55, "window", color=SHELL, fontsize=7, ha="left")

    dim(ax, 2.05, 2.45, 25.28, "land 0.40")
    dim(ax, -2.16, 0.0, 21.55, "2.16 outboard")
    dim_v(ax, 4.55, 24.30, 25.51, "ceiling", right=True)
    dim_v(ax, -2.7, 24.90, 25.80, "pocket", right=False)
    dim_v(ax, 1.15, 24.30, 24.70, "0.40", right=False)
    ax.text(-2.55, 24.85, "ear", color=NOTE, fontsize=7, ha="center")
    ax.text(2.25, 24.50, "plate", color="#333", fontsize=7, ha="center", va="center")
    ax.text(-0.2, 26.35, "shell, not a stop", color=SHELL, fontsize=8)
    ax.text(0.15, 21.4, "pad beam", color="#333", fontsize=7)
    ax.set_xlim(-3.7, 5.3)
    ax.set_ylim(20.85, 26.55)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Section through a pad  (y–z, outboard to the left)", loc="left", fontsize=11)

    # x-z along the beam. Root at the left, nub at the right.
    box(bx, 0.0, 21.05, 2.2, 1.55, CAR)                     # root wall
    box(bx, 2.05, 22.55, 1.3, 0.70, CAR)                    # root lip, 0.15 above the beam
    box(bx, 2.2, 21.05, 10.5, 1.40, PAD)                    # beam L to the nub centre
    box(bx, 11.9, 22.45, 1.4, 0.65, PAD)                    # nub
    box(bx, 0.2, 23.10, 14.2, 1.20, PANEL)
    box(bx, 11.6, 24.30, 2.0, 0.40, PLATE)
    ax2 = bx
    dim(ax2, 2.2, 12.7, 20.7, "L = 12.7 to the nub")
    dim_v(ax2, 15.5, 21.05, 22.45, "t 1.40", right=True)
    dim_v(ax2, 14.3, 22.45, 23.10, "nub", right=True)
    ax2.text(2.4, 22.85, "root lip", color=NOTE, fontsize=7)
    ax2.plot([2.2, 12.7], [23.45, 23.45], color=NOTE, lw=0.7, ls="--")
    ax2.text(6.5, 23.55, "free nub 23.45", color=NOTE, fontsize=7, ha="center")
    ax2.set_xlim(-0.4, 16.2)
    ax2.set_ylim(20.4, 26.0)
    ax2.set_aspect("equal")
    ax2.axis("off")
    ax2.set_title("Beam along the border  (x–z)", loc="left", fontsize=11)

    fig.text(
        0.01, 0.01,
        "Contact plane z = 24.30. A 1.2 mm panel is drawn. "
        "The pad pushes up on the nub. The plate pushes down on the same centre. "
        "The shell is the cover.",
        fontsize=8, color="#333")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def plan(path):
    fig, ax = plt.subplots(figsize=(13.6, 7.2), dpi=140)
    # Board coordinates, y increasing down the page so the top edge is at the top.
    x0, x1 = 29.46, 108.46
    y0, y1 = 26.24, 62.94
    box(ax, x0, y0, x1 - x0, y1 - y0, PANEL, lw=1.1)
    # Active area and the 0.50 mm window margin.
    box(ax, 32.46, 30.06, 99.36 - 32.46, 59.12 - 30.06, "#e8e8e8", lw=0.6)
    ax.add_patch(Rectangle((32.46 - 0.5, 30.06 - 0.5), 99.36 - 32.46 + 1.0,
                           59.12 - 30.06 + 1.0, fill=False, edgecolor=SHELL,
                           lw=0.8, ls="--", zorder=3))
    # PS lines, 1.52 inboard of the long edges.
    ax.plot([x0, x1], [y0 + 1.52, y0 + 1.52], color=LOAD, lw=0.5, ls=":")
    ax.plot([x0, x1], [y1 - 1.52, y1 - 1.52], color=LOAD, lw=0.5, ls=":")
    # Fences, outboard of each long edge.
    for xa, xb in ((42.0, 53.0), (84.6, 96.0)):
        box(ax, xa, y0 - 1.00, xb - xa, 0.80, CAR, lw=0.5)
    for xa, xb in ((49.6, 55.6), (76.0, 90.0)):
        box(ax, xa, y1 + 0.20, xb - xa, 0.80, CAR, lw=0.5)
    # Plates on the lands, 2.05–2.45 inboard.
    box(ax, 36.0, y0 + 2.05, 66.0, 0.40, PLATE, lw=0.7)
    box(ax, 36.0, y1 - 2.45, 66.0, 0.40, PLATE, lw=0.7)
    # Tongues and washers at the two stations on each edge.
    stations = ((47.5, y0, +1), (90.3, y0, +1), (52.6, y1, -1), (83.0, y1, -1))
    for x, edge, dire in stations:
        nub_y = edge + dire * 2.25
        box(ax, x - 0.7, nub_y - 0.4, 1.4, 0.8, PAD, lw=0.6)
        y_w = edge - dire * 0.35
        box(ax, x - 1.6, min(y_w, y_w - dire * 0.90), 3.2, 0.90, CAR, lw=0.5)
        ax.plot([x, x], [nub_y, edge - dire * 0.6], color=LOAD, lw=0.7, zorder=4)
    # One callout, top-left station.
    ax.annotate(
        "pad and plate\nsame centre\nR/H = 1",
        xy=(47.5, y0 + 2.25), xytext=(58, 18),
        fontsize=8, color=LOAD, ha="left",
        arrowprops=dict(arrowstyle="->", color=LOAD, lw=0.7))
    ax.annotate(
        "ear, outboard\nof the edge",
        xy=(47.5, y0 - 0.7), xytext=(22, 16),
        fontsize=8, color=NOTE, ha="left",
        arrowprops=dict(arrowstyle="->", color=NOTE, lw=0.7))
    ax.text(x0 + 1, (y0 + y1) / 2, "display panel", color="#333", fontsize=8, rotation=90, va="center")
    ax.text(70, 30.5, "active area", color="#555", fontsize=7, ha="center")
    ax.text(100, y0 + 1.15, "PS", color=LOAD, fontsize=7)
    ax.text(100, 29.2, "window", color=SHELL, fontsize=7)
    ax.text(36, y0 + 3.3, "plate on the land", color="#333", fontsize=7)
    ax.set_xlim(16, 116)
    ax.set_ylim(70, 12)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Plan  ·  board x to the right, board y down", loc="left", fontsize=12)
    fig.text(
        0.01, 0.01,
        "Four pads. Each nub is on the land centre, 2.25 mm inboard. "
        "The plate holds the panel down there. The washer is outboard of the edge, off the display panel. "
        "The cable leaves at the right.",
        fontsize=8, color="#333")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


def option2(path):
    fig, (ax, bx) = plt.subplots(
        1, 2, figsize=(13.8, 5.6), dpi=140,
        gridspec_kw={"width_ratios": [1.15, 1.0]})
    # Plan, local: x along the edge, y inboard positive. One leaf.
    box(ax, -1.0, -1.7, 12.5, 0.9, CAR)          # fence strip
    # Leaf from the root (outboard) toward the finger.
    ax.add_patch(Polygon(
        [(0.3, -1.2), (8.6, 1.7), (9.4, 1.3), (1.1, -1.5)],
        closed=True, facecolor=PAD, edgecolor=EDGE, lw=0.9, zorder=2))
    box(ax, 8.2, 1.85, 1.6, 0.8, PLATE)          # rigid finger on the land
    box(ax, 8.5, 2.05, 1.0, 0.4, "#f0d7a0")      # contact
    ax.plot([0.6, 9.0], [-1.2, 2.25], color=LOAD, lw=0.8, zorder=4)
    ax.plot(0.6, -1.2, "o", color=LOAD, ms=5, zorder=5)
    ax.plot(4.2, 0.3, "o", color=LOAD, ms=5, zorder=5)
    ax.plot(9.0, 2.25, "o", color=LOAD, ms=5, zorder=5)
    ax.text(0.2, -2.15, "shelf", color=LOAD, fontsize=8)
    ax.text(3.5, -0.15, "lip", color=LOAD, fontsize=8)
    ax.text(9.3, 2.7, "finger", color=LOAD, fontsize=8)
    ax.plot([-1.2, 12], [0, 0], color="#888", lw=0.6, ls="--")
    ax.text(10.3, -0.35, "panel edge", color="#555", fontsize=7)
    dim(ax, 0.4, 9.2, 3.5, "leaf L = 10")
    ax.set_xlim(-1.6, 13.2)
    ax.set_ylim(-2.6, 4.2)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Option 2 plan  ·  lip between shelf and finger", loc="left", fontsize=11)

    # Section at the finger. Leaf is outboard and lower. Finger is rigid.
    box(bx, -2.4, 21.4, 1.2, 4.3, CAR)            # root block outboard
    box(bx, -1.2, 22.6, 3.4, 1.33, PAD)           # leaf, t 1.33, beside the edge
    box(bx, 1.6, 23.5, 1.5, 0.80, PLATE)          # rigid finger rising onto the land
    box(bx, 2.05, 24.30, 0.40, 0.40, PLATE)
    box(bx, 0.0, 23.10, 4.2, 1.20, PANEL)
    box(bx, 2.05, 22.70, 0.40, 0.40, CAR)         # fixed pad under the finger
    box(bx, -0.2, 25.51, 3.6, 0.74, SHELL)
    ax3 = bx
    arrow(ax3, 2.25, 23.55, 0, 0.55, "pad", 2.6, 23.7)
    arrow(ax3, 2.25, 25.2, 0, -0.35, "finger", 2.6, 25.15, va="bottom")
    ax3.text(-0.4, 23.1, "leaf", color="#333", fontsize=7)
    ax3.text(2.6, 24.5, "rigid", color="#333", fontsize=7)
    ax3.set_xlim(-2.8, 4.8)
    ax3.set_ylim(21.1, 26.5)
    ax3.set_aspect("equal")
    ax3.axis("off")
    ax3.set_title("Option 2 section  ·  finger over the land", loc="left", fontsize=11)
    fig.text(
        0.01, 0.01,
        "Second option. The leaf is outboard of the edge, 1.6 mm wide and 10 mm long. "
        "The finger is rigid. The pad under the finger is fixed. "
        "Shelf, lip, and finger are on one line.",
        fontsize=8, color="#333")
    fig.tight_layout(rect=(0, 0.05, 1, 1))
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    section(os.path.join(OUT, "concept-pad-section.png"))
    plan(os.path.join(OUT, "concept-pad-plan.png"))
    option2(os.path.join(OUT, "concept-option2-outboard.png"))
