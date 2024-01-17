ISB TESTING:

This folder contains the SST implementation of the ISBs using forza specific libraries. To run the tests make sure the **forza** library is installed. 

STEPS TO RUN TEST:
-  cd <test_folder>
-  make
-  sst rev-test.py

WHAT DO THE TESTS RUN?
-  TC: runs triangle count on a scale 8 graph using four actors.
       Result should be 10851 triangles
-  Jaccard: runs a jaccard similarity index on a 50x50 input matrix with 50x50 kmer matrix.
            Result will contain lvalues:
            LVAL: [0] S(13, 1) = 0.2500000000
            LVAL: [0] S(22, 1) = 0.5000000000
-  BFS: runs a top-down bfs on a 40x40 graph using 4 actors.
        Result contains the print that all nodes have been visited.
        If algorithm fails, it will print the nodes that have not been visited.

TEST EXTENSION:

To run your own tests, the generated graph test files need to be placed in the respective <test_folder> and to change the number of actors, it can be modified in the params field of rev-test.py.
