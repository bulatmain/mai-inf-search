#!/usr/bin/env python3
import sys
import os


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 zipf_plot.py <tokens_file> <output_png>")
        sys.exit(1)

    tokens_file = sys.argv[1]
    output_png = sys.argv[2]

    freq = {}
    total = 0
    with open(tokens_file, "r", encoding="utf-8") as f:
        for line in f:
            token = line.strip()
            if token:
                freq[token] = freq.get(token, 0) + 1
                total += 1

    sorted_freq = sorted(freq.values(), reverse=True)
    ranks = list(range(1, len(sorted_freq) + 1))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    fig, ax = plt.subplots(figsize=(10, 7))

    ax.loglog(ranks, sorted_freq, 'b-', linewidth=0.8, alpha=0.7, label="Observed frequency")

    C = sorted_freq[0]
    zipf_theoretical = [C / r for r in ranks]
    ax.loglog(ranks, zipf_theoretical, 'r--', linewidth=1.5, alpha=0.8,
              label=f"Zipf's law: f = {C}/{'{r}'}")

    ax.set_xlabel("Rank (log scale)", fontsize=12)
    ax.set_ylabel("Frequency (log scale)", fontsize=12)
    ax.set_title("Zipf's Law: Term Frequency Distribution\n(Movie Reviews & Cinema News Corpus)", fontsize=14)
    ax.legend(fontsize=11)
    ax.grid(True, alpha=0.3)

    stats_text = (f"Unique terms: {len(freq):,}\n"
                  f"Total tokens: {total:,}\n"
                  f"Max frequency: {sorted_freq[0]:,}\n"
                  f"Min frequency: {sorted_freq[-1]:,}")
    ax.text(0.98, 0.98, stats_text, transform=ax.transAxes, fontsize=9,
            verticalalignment='top', horizontalalignment='right',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    plt.tight_layout()
    plt.savefig(output_png, dpi=150)
    print(f"Zipf plot saved to {output_png}")
    print(f"Unique terms: {len(freq)}")
    print(f"Total tokens: {total}")


if __name__ == "__main__":
    main()
