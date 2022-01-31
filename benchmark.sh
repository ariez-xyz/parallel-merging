module load mpt
cd /home/d3000/d300342/mpicomm
for i in 4 8 16 32 48 64 96 128 192; do timeout -s INT 7200 mpiexec -perhost $i ./mpicomm test/pokec_1idx.metis test/pokec-comms.nl > run$i.out; done
