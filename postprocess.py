#!/usr/bin/python3

import sys

if len(sys.argv) == 3:
    filename = sys.argv[2]
    original = sys.argv[1]
    map_nodes = True
elif len(sys.argv) == 2:
    filename = sys.argv[1]
    map_nodes = False
else:
    print("usage:\tpython {sys.argv[0]} (original_metis outfile) | outfile")
    print("\tif two params are passed, community node ids will be remapped to work with the original file (in case it contained isolated nodes)")
    quit()



if map_nodes:
    nodemap = {}
    with open(original) as f:
        f.readline()
        line_num = 1
        node_id = 0
        for line in f.readlines():
            if line.strip() != "":
                nodemap[str(node_id)] = line_num
                node_id += 1
            line_num += 1





with open(filename) as f:
    for line in f.readlines():
            if "community" in line:
                if "at" in line:
                    print("ERROR:", line)
                    quit()
                nodes = line.split(":")[2].strip()
                if map_nodes:
                    split = nodes.split()
                    for node in split[:-1]:
                        print(nodemap[node], end=" ")
                    print(nodemap[split[-1]])
                else:
                    print(nodes)

