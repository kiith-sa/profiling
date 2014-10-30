.. Packages needed:
.. 
.. g++/clang with -std=c++11
.. time
.. valgrind (only callgrind needed)
.. kcachegrind
.. graphviz
.. linux-tools-common / linux-tools-generic
.. some sort of diff


.. role:: footnote

=========
Profiling
=========

:Author:
    Ferdinand Majerech

https://github.com/kiith-sa/profiling


-----
Intro
-----

* Your program is slow.

  - If it works, it can work 50 times faster\*
* If you think you know why it's slow, **you're wrong**\*.

* Don't start optimizing your slow code.

  - You will end up optimizing something that's not slow

* Start by **profiling**, then optimize.

:footnote:`\* in most cases`


--------
Overview
--------

* Things you should know
* Files
* Tools

  - Feel free to experiment, slides will be online
* Appendices: Optimization & links


---------------------------
Real world machines - cache
---------------------------

* CPU is fast, RAM is slow, and far away
* Need a cache

  - Cache is fast, RAM is slow

    * Need L2 cache

      - L2 is fast, RAM is slow -> L3

        * ...

.. figure:: /_static/caches.png
  :align: center

------------------------------
Real world machines - branches
------------------------------

* CPUs process instructions in a pipeline

* Need to **predict** which path a **branch** will take
* Branch prediction fail example::

     for (int c = 0; c < arraySize; ++c) {
         if (data[c] >= 128) sum += data[c];
     }

  Time for random data: 11.777s

  Time for sorted data: 2.352s

----------------------------------------
Numbers everyone should know (Jeff Dean)
----------------------------------------

==================================== =============== =================
L1 cache hit (data not in register)  ~0.5 ns         few cycles
Branch mispredict                    ~5 ns
Lmax cache reference (L1/L2 miss)    ~7 ns           ~14x L1 hit
Mutex lock/unlock                    ~25 ns
Main memory reference (Lmax miss)    ~100 ns         ~14x L2 hit
Compress 1K bytes w/ Snappy (Google) ~3,000 ns
Send 2K bytes over 1 Gbps network    ~20,000 ns
Read 4k random read from SSD         ~150,000 ns
Read 1 MB sequentially from RAM      ~250,000 ns
Round trip within same datacenter    ~500,000 ns
Read 1 MB sequentially from SSD      ~1,000,000 ns
Disk seek                            ~10,000,000 ns
Read 1 MB sequentially from disk     ~20,000,000 ns  ~80x RAM + seek
Send packet CA->Netherlands->CA      ~150,000,000 ns
==================================== =============== =================


=====
Files
=====

======================= ======================================
``commands.txt``        Commands for copy pasting
``small.cfg``           Small CFG file for testing
``huge.cfg``            Huge CFG file for testing
``cfg*.cpp``/``cfg*.h`` Sample code (CFG parser) for profiling
``diy.h``               DIY profiling example
``LICENSE_1_0.txt``     Boost license
======================= ======================================

------------------------------
Building the binary to profile
------------------------------

* Use ``-std=c++11``
* Optimized ``-O2`` build w/ debug info ``-g``
* Builds:

  ============= ====================================================
  Debug:        ``g++ cfg.cpp -std=c++11 -g -o cfg``
  Profiling:    ``g++ cfg.cpp -std=c++11 -g -O2 -fno-inline -o cfg``
  Benchmarking: ``g++ cfg.cpp -std=c++11 -g -O2 -o cfg``
  ============= ====================================================

* Running: ``./cfg FILE.cfg [PARSECOUNT]``

-----------
On inlining
-----------

* Decreases profile readability, improves performance

  - cascading optimizations

* Decreases the number of instructions **executed**
* Increases **binary size** (may hurt icache)


-------------------------
Evolution of a cfg parser
-------------------------


* JIT-ed langs: base performance often *very good*

  but if *very good* is not *good enough*, you're fucked

* Native langs: can always go lower level (until everything's ASM)

* ``cfg``: "Obvious" C++ solution
* ``cfg2``: Removing overhead of ``std::map``
* ``cfg3``: Avoiding copies with string ops using slices
* ``cfg4``: Decreasing string overhead with plain C strings
* ``cfg5``: No internal allocations (except loading the file).

-------------------------
Evolution of a cfg parser
-------------------------

==== =========== ===============
bin  huge.cfg 10 small.cfg 50000
==== =========== ===============
cfg  ~1290ms     ~1070ms
cfg2 ~880ms      ~1010ms
cfg3 ~700ms      ~700ms
cfg4 ~445ms      ~605ms
cfg5 ~420ms      ~595ms
==== =========== ===============

* Could go further

  - Dense map or rewriting STL sort/search
  - Rewriting ``strlen``
  - Using SSE intrinsics
  - Rewriting chunks of code in ASM



=====
Tools
=====

* ``time``
* ``valgrind`` family
* ``perf`` family
* ``clock_gettime()`` (DIY)

----
time
----

* Duh
* Benchmarking
* ``time [COMMAND]``

``time ./cfg small.cfg 200``::

   real 0m0.011s
   user 0m0.009s
   sys  0m0.002s

-----
gprof
-----

.. rst-class:: centered

   **Do not use gprof.**\*

:footnote:`* except to get function call counts`

--------
valgrind
--------

* Runs an application in a VM
* Slowly

* ``valgrind`` tools:

  - ``memcheck``   - debugging, not profiling
  - ``cachegrind`` - cache simulation
  - ``callgrind``  - cache simulation + better profiling
  - ``massif``     - memory profiling
  - ...

* ``kcachegrind`` - ``callgrind``/``cachegrind`` viewer

---------
callgrind
---------

* ``valgrind --tool=callgrind [COMMAND]``
* Prints info and dumps ``callgrind.out.[NUMBER]``

  - read with ``kcachegrind``
* The VM catches everything - no need for multiple runs
* Not always realistic (**instructions** are not **cycles**)
* Not useful when making a lot of async / syscalls
* **Very** useful otherwise, esp. with ``kcachegrind``

-------------------
callgrind - example
-------------------

``valgrind --tool=callgrind ./cfg huge.cfg 1``::

   I   refs:      757,793,107
   I1  misses:          4,312
   LLi misses:          3,776
   I1  miss rate:         0.0%
   LLi miss rate:         0.0%

   D   refs:      358,393,103  (212,485,868 rd + 145,907,235 wr)
   D1  misses:      2,020,202  (  1,864,991 rd +     155,211 wr)
   LLd misses:      1,491,826  (  1,341,624 rd +     150,202 wr)
   D1  miss rate:         0.5% (        0.8%   +         0.1%  )
   LLd miss rate:         0.4% (        0.6%   +         0.1%  )

   LL refs:         2,024,514  (  1,869,303 rd +     155,211 wr)
   LL misses:       1,495,602  (  1,345,400 rd +     150,202 wr)
   LL miss rate:          0.1% (        0.1%   +         0.1%  )

   Branches:       93,924,200  ( 75,266,948 cond +  18,657,252 ind)
   Mispredicts:     8,760,703  (  4,822,612 cond +   3,938,091 ind)
   Mispred rate:          9.3% (        6.4%     +        21.1%   )

-----------
KCachegrind
-----------

* ``callgrind`` dump viewer
* Call graph, callee map, per line/instruction events
* **Cycle estimation** - more realistic than **instructions**

.. figure:: /_static/kcachegrind.png
  :width: 80%
  :align: center

--------------------
KCachegrind examples
--------------------

``kcachegrind callgrind.out.[NUMBER]``

* Profile (and see diffs between) versions of ``cfg.cpp``

* Top-down:

  - Select ``main()`` in the ``Flat Profile``
  - View its annotated source code
  - Descend into slowest callees

* Bottom-up:

  - Sort ``Flat Profile`` by ``Self`` to find most expensive function
  - Look at its ``Call Graph`` (``graphviz`` needed) or ``All Callers``
  - Think about how to optimize or avoid calling it

* View two event types at once (``View > Secondary Event Type``)


------
massif
------

* Profiles memory allocation/usage
* ``massif-visualizer`` for GUI
* No time for that now

.. figure:: /_static/massif-visualizer.png
  :width: 80%
  :align: center

-------------
~/.valgrindrc
-------------

.. code::

   --num-callers=20
   --callgrind:compress-strings=no
   --callgrind:dump-line=yes
   --callgrind:dump-instr=yes
   --callgrind:compress-pos=no
   --callgrind:separate-threads=no
   --callgrind:collect-jumps=yes
   --callgrind:collect-systime=yes
   --callgrind:collect-bus=yes
   --callgrind:cache-sim=yes
   --callgrind:branch-sim=yes
   --callgrind:I1=32768,8,64
   --callgrind:D1=32768,8,64
   --callgrind:LL=262144,8,64

----
perf
----

* Frontend to kernel ``perf-events``
* Very comphrehensive
* Virtually nonexistent docs and sorta passable UI
* Measures **real** performance
* Uses HW(CPU-dependent) and kernel counters


---------------
perf - commands
---------------

* ``list`` - list profiling events
* ``stat`` - overhead summary
* ``record`` - record profiling events - ``report`` - view results
* ``top`` - profile in real time
* ...


---------
perf list
---------

``perf list`` lists events ``perf`` can record on this machine

Some interesting events:

========================= ========================================
cycles                    CPU cycles spent
instructions              Instructions executed
branches                  All branches
branch-misses             Mispredicted branches (~5ns)
L1-Xcache-XXXX-misses     L1 cache misses (<=~7ns)
cache-references          Lmax cache accesses (~7ns)
cache-misses              Lmax cache misses (~100ns)
stalled-cycles-frontend   Cycles the CPU frontend is doing nothing
stalled-cycles-backend    Cycles the CPU itself is doing nothing
========================= ========================================

---------
perf stat
---------

``perf stat ./cfg small.cfg 1000``::

   Performance counter stats for './cfg small.cfg 1000':

           26,554473 task-clock (msec)       #    0,965 CPUs utilized
                  13 context-switches        #    0,490 K/sec
                   5 cpu-migrations          #    0,188 K/sec
                 356 page-faults             #    0,013 M/sec
          77 375 671 cycles                  #    2,914 GHz
          27 928 664 stalled-cycles-frontend #   36,09% frontend cycles idle
     <not supported> stalled-cycles-backend
         121 540 390 instructions            #    1,57  insns per cycle
                                             #    0,23  stalled cycles per insn
          30 904 551 branches                # 1163,817 M/sec
             356 636 branch-misses           #    1,15% of all branches

---------
perf stat
---------

* See ``commands.txt`` for an example on how to get cache miss stats
* Gets precise stats
* If ``stalled-cycles-XXX`` is low
* How common are branch mispredicts?
  
  Branch miss count / branch count
* How common are cache misses?

  Cache miss count / instruction count




-----------
perf record
-----------

* ``perf record -gF10000 ./cfg huge.cfg 2``

  - Record cycles at 10000Hz (``F``)requency with call(``g``)raph info
  - Higher frequency - more distorded profile resulits
  - Best to record at low frequency for a long time

* Dumps ``perf.data`` to view with ``perf report`` (next slide)

* See ``commands.txt`` for recording events other than ``cycles``


-----------
perf report
-----------

* ``perf report`` reads ``perf.data`` in current directory
* Shows all recorded event types (just ``cycles`` by default)
* Can drill down into lines/instructions like ``kcachegrind``
* Controls: 

  ====================== =================================================
  ``<Right>``/``<Left>`` more details/back
  ``<Up>``>/``<Down>``   next/previous function
  ``<Enter>``            call graph for selected function (pretty useless)
  ``/``                  Filter functions
  ====================== =================================================

* Note: misses, mispredicts can show a few lines below the source

  - Stalls are not detected immediately by CPU

--------
perf top
--------

* Real-time ``perf record`` + ``perf report``
* Accumulates recorded events

  - According to docs, ``z`` resets... docs are out of date
* System-wide - drill into a relevant function to select a thread
* Great for intensive event-driven applications (e.g. games)
* **Needs to be run as root**

------------------
perf top - example
------------------

* In one terminal (or background) run:

  ``./cfg huge.cfg 20000``

* In another terminal, run:

  ``sudo perf top -F10000`` (or equivalent; run ``perf top`` as root)

  - No call graph info

* Terminate the first process when done



---
DIY
---

* See ``diy.h``
* high-precision clocks

  - ``clock_gettime()`` on POSIX (``#include <time.h>``)
* Use with RAII (ctor+dtor) to record time elapsed in a zone 
* Add ``PRINT_ZONES = true`` to ``main()`` in ``cfg.cpp`` and run ``cfg``


---------
DIY - why
---------

* ``perf`` offers much better performance w/o instrumentation
* But: ``perf`` averages results (even ``perf top``)
* Problem: rare overhead spikes (e.g. lag in a game)
* Need to keep track of **when** overhead occurs, not only **where**
* Instrument code with RAII zones, record overhead individually



--------
Despiker
--------

* (DIY) real-time hierarchical frame profiler
* GitHub project(D): https://github.com/kiith-sa/despiker

.. raw:: html

   <video preload="auto" autoplay controls loop poster="../_static/despiker-preview.png">
      <source src="_static/despiker.webm" type="video/webm">
   </video>


====================
Appendix 1: Patterns
====================

* Non-evil optimizations: don't kill maintainability

--------------
Patterns - STL
--------------

* Avoid ``std::list``. Use ``std::vector``.

  - Avoid linked lists in most cases

* Avoid ``std::map``. ``std::unordered_map`` sucks too.

  - Use a sorted ``std::vector`` if not too cumbersome
  - Or find a dense hash map lib
  - For small integer-indexed maps, use an index table

* Keep in mind the costs of destructors of STL objects


----------------
Patterns - Cache
----------------

* Avoid too many indirections (pointers to pointers to pointers).
* Keep data packed tightly together (alignment).

=========================
Appendix 2: Optimizations
=========================

* Are not useful unless you profile first


----------------------------
Optimizations - polymorphism
----------------------------

* Run-time: (Child classes overriding parent's virtual methods)

  - Dereferences **vtable** pointer, then **function** pointer
  - Inlining not possible
  - On some platforms almost always a cache miss (ARM?)

* "Cheap" polymporhism (manual vtable in class body)

  - One dereference instead of two

* Compile-time polymporhism with templates

  - Extremely ugly in C++, google ``C++ crtp``
  - Direct call, allows inlining

---------------------
Optimizations - cache
---------------------

* Keep things at the first (x86: 64B) cache line

.. code-block:: d

    class Bad
    {
        uint64_t[8] rarelyAccessed;
        // cache line (64 bytes) boundary
        std::string alwaysAccessed:
    }

    class Good
    {
        // Fits the first cache line
        std::string alwaysAccessed:
        uint64_t[8] rarelyAccessed;
    }


---------------------------------
Optimizations - branch prediction
---------------------------------

Bad::

   if (data[c] >= 128) sum += data[c];

Good::

   int t = (data[c] - 128) >> 31;
   sum += ~t & data[c];

-------------------------------------
Optimizations - alloc (time) overhead
-------------------------------------

* Preallocate (e.g. ``std::vector::reserve()``)
* GC if many allocs sum up to little memory
* ``alloca``
* Fixed-size stack arrays with a heap fallback for large sizes
* Free lists to recycle instances (next slide)
* Region allocators

  http://en.wikipedia.org/wiki/Region-based_memory_management

------------------------------
Optimizations - alloc overhead
------------------------------

.. code-block:: cpp

   class Foo {
       static Foo* freelist; // Start of free list

       static Foo* alloc() {
           Foo* f;
           if(freelist) { // Reuse from freelist
               f        = freelist;
               freelist = f.next;
           }
           else {
               f = new Foo();
           }
           return f;
       }

       static void dealloc(Foo* f) { // Move to freelist
           f.next   = freelist;
           freelist = f; 
       }

       Foo* next; // For use by freelist
       // Other Foo code here...
   }

-------------------------------
Optimizations - compiler params
-------------------------------

* ``-O2``: sane default
* ``-O3``: better or worse than ``-O2``
* ``-Os``: often better than ``-O2``

* GCC/Clang have many other optimization flags

  - Different flags work for different applications


------------------------
Optimizations - inlining
------------------------

* ``inline __attribute__((__always_inline_))``


  - MSVC: ``__forceinline``
  - still not always inlined

* ``__attribute__((__noinline__))``

  - MSVC: ``__declspec(noinline)``
  - sometimes inlined


--------------------------
Optimizations - intrinsics
--------------------------

* Very compiler/architecture specific
* Do 4/8/16 operations per cycle
* All x64 CPUs support SSE2

  - 128b (4 floats/ints, 8 shorts, 16 bytes)
* New-ish CPUs support AVX

  - 256b, 512b (AVX2)
* ARM: NEON
* http://www.linuxjournal.com/content/introduction-gcc-compiler-intrinsics-vector-processing?page=0,0

-------------------------
Optimizations - int types
-------------------------

* Use the native 32/64bit type

  - unless cache perf can be improved (often with big int arrays)
* ``#include <cstdint>``: http://en.cppreference.com/w/cpp/types/integer
* 16bit seems to result in worst perf on x86

  - But not if the saved memory reduces cache misses


---------------------------------------
Optimizations - 0 (Andrei Alexandrescu)
---------------------------------------

* 0 is special (on x86, and supposedly on ARM too)
* Specific instructions handle comparisons with 0
* Make the most common values 0 so you can compare with 0

  - E.g. default value of an ``enum``


=================
Appendix 3: Links
=================


------------
Links - perf
------------
* https://perf.wiki.kernel.org/index.php/Tutorial
* http://paolobernardi.wordpress.com/2012/08/07/playing-around-with-perf/
* http://web.eece.maine.edu/~vweaver/projects/perf_events/perf_event_open.html
* http://www.brendangregg.com/perf.html

-----------------------------
Links - other Linux profilers
-----------------------------

* RotateRight Zoom (commercial): http://www.rotateright.com/
* OProfile (open source):        http://oprofile.sourceforge.net/news/
* Intel VTune (commercial):      https://software.intel.com/en-us/intel-vtune-amplifier-xe
* AMD CodeXL (freeware):         http://developer.amd.com/tools-and-sdks/opencl-zone/codexl/
* SysProf (open source):         http://sysprof.com/

--------------------
Links - optimization
--------------------

* Branch prediction:

  http://stackoverflow.com/questions/11227809/why-is-processing-a-sorted-array-faster-than-an-unsorted-array/11227902#11227902

* Drepper (Cache and memory)

  http://www.akkadia.org/drepper/cpumemory.pdf

* Fog

  http://agner.org/optimize/

  http://www.agner.org/optimize/blog/

---------------------------------
Links - GCC optimization features
---------------------------------

* Often compatible with Clang

https://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html#Function-Attributes

https://gcc.gnu.org/projects/prefetch.html

-------------------------
Links - Intel/AMD manuals
-------------------------

* Intel Software Optimization Reference Manual:

  http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf

* Software Optimization Guide for AMD Family 15h (and newer...):

  http://amd-dev.wpengine.netdna-cdn.com/wordpress/media/2012/03/47414_15h_sw_opt_guide.pdf

* All Intel software developer manuals:

  http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html

* All AMD software developer manuals:

  http://developer.amd.com/resources/documentation-articles/developer-guides-manuals/

--------------
Links - CPPCon
--------------

* Alexandrescu:

  https://www.youtube.com/watch?v=Qq_WaiwzOtI

* Carruth:

  https://www.youtube.com/watch?v=fHNmRkzxHWs&list=UUMlGfpWw-RUdWX_JbLCukXg

* All: https://www.youtube.com/user/CppCon/videos


=========
It's over
=========

https://github.com/kiith-sa/profiling
