for f in $(ls *metis); do echo $f; ./preprocess.py $f 3; mv out $f; done
for f in $(ls *nl); do echo $f; ./preprocess.py $f 3; mv out $f; done

