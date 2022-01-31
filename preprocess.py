#!/usr/bin/python3

"""
Preprocess an .nl or .metis file so that it works with the mpicomm tool suite
"""

import sys
import networkit as nk

if len(sys.argv) != 3:
	print("usage: {sys.argv[0] infile min_community_size}")
	exit()

# remove unconnected nodes!
# this requires postprocessing with a suitable postprocess script (as is in this dir)
if sys.argv[1].split(".")[-1] == "metis":
    g = nk.readGraph(sys.argv[1], nk.Format.METIS)
    g.forNodes(lambda n: g.removeNode(n) if g.degree(n) == 0 else None)
    nk.writeGraph(g, "preprocess_tmpfile.metis", nk.Format.METIS)
    sys.argv[1] = "preprocess_tmpfile.metis"

with open(sys.argv[1]) as f:
    with open("out", "w") as of:
        if sys.argv[1].split(".")[-1] == "nl":
            # Remove spaces and sort
            for line in f:
                nodes = sorted([int(x) for x in line.split()])
                if len(nodes) > int(sys.argv[2]):
                    print("".join(map(lambda x: str(x) + " ", nodes)).strip())

        elif sys.argv[1].split(".")[-1] == "metis":
            # Remove spaces and sort all lines except for header
            s = f.readline().strip().split()
            of.write(" ".join(s[:2]))
            of.write("\n")
            for line in f.readlines():
                content = sorted([int(x) for x in line.split()])
                s = ""
                for item in content:
                    s += str(item) + " "

                of.write(s.strip())
                of.write("\n")

