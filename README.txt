-- Initializations --

The data received as arguments is processed and stored in the GlobalData
structure, which contains all the variables that would otherwise have
been used globally. The thread_args vector of pairs is used to separate
the arguments specific to each thread. A field for the thread ID was
not included in the GlobalData structure to avoid overwriting, since
it is a variable that we do not want all threads to access concurrently.

-- get_files --

The input file containing all the other files from which words will be
extracted is processed. These files are added to a queue so they can be
dynamically distributed among the mappers.

-- map_files --

The vector of partial lists is filled. Its size was set to exactly M,
since each mapper generates its own list. A local array of words is
created to avoid the overhead caused by locking and unlocking if we
were to work directly with partial_lists, which is accessible to all
threads. However, locking is still necessary when each thread retrieves
a file from the queue. Thus, while one thread is busy processing a
larger file, the others can continue retrieving files from the queue
and processing smaller files in parallel. At this stage, only word-file
pairs are created; sorting and duplicate elimination will be handled
in later steps.

Also at this stage, the characters of each word are converted to
lowercase, and other characters such as _ or ' are removed.

-- reduce_files --

Certain portions of the vector of partial lists are distributed among
the reducers. Each thread copies the pairs from the partial lists into
a local map. A map is used to eliminate duplicates. I also chose a local
variable because the number of lock and unlock operations would have
been very high if we had worked directly with the global map, since
there are more iterations at this stage compared to the step in which
words are copied from the local map into the global one. This is also
the stage in which a set of file indices is created.

At this point, each reducer thread has generated its own map
corresponding to its portion of words. Locking is used because each
thread needs to copy the word-ID pairs into the global map, which
represents a critical section. I also used a map here because, when
inserting a word, it will be found in the map if it already exists,
and its set of indices will automatically be updated. I used a set
for the indices so that they are sorted in ascending order when added
and duplicates are eliminated.

The resulting words_map contains all the words with their complete sets
of indices, but we must ensure that all threads have finished processing
their portions before proceeding with the map, so a barrier is used.

To sort the words beginning with the same letter according to the
number of indices, they are moved into vectors specific to each letter
of the alphabet, which are then sorted accordingly.

These vectors are then distributed again among the threads, and the
words are written to the files corresponding to each letter.

-- Working with Barriers --

I used a separate barrier of size M + R to ensure that the reducer
threads do not begin execution until all mapper threads have completed
their tasks. The barrier of size R is used only during reduce operations.
