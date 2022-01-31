# mpicomm

Tool for highly parallel merging of clusters in graphs.

## Assumptions

.metis and .nl files:

* are sorted 
* do not have extra spaces at EOL 
* have a newline at EOF
* do not contain empty lines

The preprocess scripts take care of these requirements.


## Usage

1. Get initial clustering via HiDALGO-pipeline
2. Use `preprocess.py` to prepare input data
3. Run this
4. If needed, increment node ids by 1 using `postprocess.py` (metis is 1-indexed)


