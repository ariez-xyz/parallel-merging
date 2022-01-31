# mpicomm

Tool for highly parallel merging of clusters in graphs.

## Assumptions

* .metis and .nl files:
  * are sorted 
  * do not have extra spaces at EOL 
  * have a newline at EOF
  * do not contain empty lines
* Merged communities' data is assigned to both child communities, but the index isn't updated any further, so the same community might appear a bunch of times in the index
* Will have to figure out how big of a problem that is

## Usage

1. Get initial clustering via HiDALGO-pipeline
2. Use `preprocess.py` to prepare input data
3. Run this
4. Increment node ids by 1 using `postprocess.py`

## API

`fromMetis(FILE* f)`

Read `f` into a `graph` struct. 

`matrix *subgraph(graph *g, community *c)`

Create adjacency matrix of induced subgraph of `c` in `g`. 
WARNING: Assumes the edges of `g` are sorted! 
This has to be done before calling `fromMetis`, e.g. by using `sortMetis.py`.

