# lib-face

A very fast auto-complete server; to be used for as-you-type search suggestions.

I have written a [blog post](http://dhruvbird.blogspot.com/2010/09/very-fast-approach-to-search.html) comparing various methods for implementing auto-complete for user queries.

lib-face implements *Approach-4* as mentioned in the blog post. The total cost of querying (TCQ) a corpus of 'n' phrases for not more than 'k' frequently occurring phrases that share a prefix with a supplied phrase is O(k log n). This is close the best that can be done for such a requirement. lib-face also provides an option to switch to using another (faster) algorithm that uses more memory but results in a per-query run-time of O(k log k).

The task of implementing a better RMQ data structure that takes up O(N) memory and O(1) query time is work in progress. Contributions are most welcome. To read up on RMQ (Range Maximum Query), see [here](http://community.topcoder.com/tc?module=Static&d1=tutorials&d2=lowestCommonAncestor#A%20O%28N%29,%20O%281%29%20algorithm%20for%20the%20restricted%20RMQ) and [here](http://www.topcoder.com/tc?module=LinkTracking&link=http://www.math.tau.ac.il/~haimk/seminar04/LCA-seminar-modified.ppt&refer=)

lib-face is written using C++ and uses the in-process [Mongoose Web Server](http://code.google.com/p/mongoose/) to serve requests.

Visit the [QuickStartGuide] to get started now!

See the [Benchmarks]!

Read the [paper](http://dhruvbird.com/autocomplete.pdf)!
