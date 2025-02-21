# CASTAN: Cycle Approximating Symbolic Timing Analysis for Network Function

## Overview

Software network functions promise to simplify the deployment of network services and reduce network operation cost.
However, they face the challenge of unpredictable performance.
Given this performance variability, it is imperative that during deployment, network operators consider the performance of the NF not only for typical but also adversarial workloads.
We contribute a tool that helps solve this challenge: it takes as input the LLVM code of a network function and outputs packet sequences that trigger slow execution paths.
Under the covers, it combines directed symbolic execution with a sophisticated cache model to look for execution paths that incur many CPU cycles and involve adversarial memory-access patterns.
We used our tool on 11 network functions that implement a variety of data structures and discovered workloads that can in some cases triple latency and cut throughput by 19% relative to typical testing workloads.

For more information, the read the [paper](castan.pdf), or go to the [project website](https://pedrosa.2y.net/Projects/CASTAN).

<p align="center">
  <img src="artifacts_available.jpg" width="100px" title="Artifacts Available: this badge indicates that author-created artifacts relevant to this paper have been placed on a publicly accessible archival repository" />
  <img src="artifacts_evaluated_functional.jpg" width="100px" title="Artifacts Evaluated - Functional: this badge indicates that the artifacts associated with the research are found to be documented, consistent, complete, exercisable, and include appropriate evidence of verification and validation." />
  <img src="artifacts_evaluated_reusable.jpg" width="100px" title="Artifacts Evaluated - Reusable: this badge indicates that the artifacts associated with the paper are of a quality that significantly exceeds minimal functionality." />
</p>


## Source Code

CASTAN was developed as a fork of [KLEE](https://klee.github.io/) and so follows a similar code structure.
At a high level, code is organized as follows:

 * [examples/](examples/) - NF code to be analyzed.
 * [include/](include/) - Header files.
 * [lib/](lib/) - KLEE and CASTAN libraries.
 * [tools/](tools/) - Main files for the final executables.

The core components of CASTAN are:

 * The CPU cache model ([ContentionSetCacheModel.cpp](lib/CASTAN/ContentionSetCacheModel.cpp)).
 * The directed symbolic execution heuristic ([CastanSearcher.cpp](lib/CASTAN/CastanSearcher.cpp)).
 * Havoc reconciliation ([castan.cpp](tools/castan/castan.cpp), within the KleeHandler::processTestCase function).

Additionally, several NFs were implemented and analyzed (in the examples/ directory):

 * [dpdk-lb-basichash](examples/dpdk-lb-basichash): Load Balancer implemented with a hash table.
 * [dpdk-lb-hashring](examples/dpdk-lb-hashring): Load Balancer implemented with a hash ring.
 * [dpdk-lb-stlmap](examples/dpdk-lb-stlmap): Load Balancer implemented with a red-black tree.
 * [dpdk-lb-tree](examples/dpdk-lb-tree): Load Balancer implemented with an unbalanced tree.
 * [dpdk-lpm-btrie](examples/dpdk-lpm-btrie): Longest Prefix Match implemented with a PATRICIA trie.
 * [dpdk-lpm-da](examples/dpdk-lpm-da): Longest Prefix Match implemented with a lookup table.
 * [dpdk-lpm-dpdklpm](examples/dpdk-lpm-dpdklpm): Longest Prefix Match implemented with a hierarchical lookup table.
 * [dpdk-nat-basichash](examples/dpdk-nat-basichash): Network Address Translator implemented with a hash table.
 * [dpdk-nat-hashring](examples/dpdk-nat-hashring): Network Address Translator implemented with a hash ring.
 * [dpdk-nat-stlmap](examples/dpdk-nat-stlmap): Network Address Translator implemented with a red-black tree.
 * [dpdk-nat-tree](examples/dpdk-nat-tree): Network Address Translator implemented with an unbalanced tree.
 * [dpdk-nop](examples/dpdk-nop): NOP network function.


## Building CASTAN

### TL;DR:

We have prepared a simple [install script](install.sh) that prepares all of CASTAN's build dependencies and builds CASTAN itself.
It is difficult to make such scripts in a general way that works for all user environments so the script is kept simple to make it easy to edit as needed for the user's specific circumstances.
The script currently assumes and has been tested on a fresh Ubuntu Server 16.04 installation.
Simply clone the CASTAN repo and run the script to have a working environment:

    $ git clone https://github.com/nal-epfl/castan.git
    $ ./castan/install.sh

The script sets up environment variables from .bashrc so it is a good idea to close and reopen the terminal after running it.

We have also prepared a [Dockerfile](Dockerfile) that copies the locally cloned CASTAN repo into a fresh Ubuntu 16.04 container and runs the install script to create a working environment within a container:

    $ cd castan
    $ docker build -t castan .
    $ docker create --name castan -it castan
    $ docker start -ai castan

### Understanding the build process

CASTAN is built as a fork of KLEE and thus has similar build requirements.
Though it is possible to build on other platforms, we document here the build process for Ubuntu Server 16.04.
The build prerequisites are:

 * Ubuntu packages: autoconf, automake, bc, binutils-dev, binutils-gold, bison, build-essential, cmake, curl, doxygen, flex, g++, gcc, git, libboost-all-dev, libcap-dev, libffi-dev, libgoogle-perftools-dev, libncurses5-dev, libpcap-dev, libtcmalloc-minimal4, libtool, libz3-dev, m4, make, parallel, python, python-minimal, python-pip, subversion, texinfo, unzip, wget, zlib1g, zlib1g-dev
 * LLVM 3.4 and CLang 3.4 (no need for compiler-rt), which we build from source.
 * STP 2.1.2, which we build from source.
 * MiniSAT (master branch, or commit 3db58943b6ffe855d3b8c9a959300d9a148ab554 in our latest build), which we build from source.
 * KLEE uClibC (branch: klee_0_9_29), which we build from source.

We build CASTAN with the following commands (adapt as needed, particularly the dependency folders):

    $ CXXFLAGS="-std=c++11" \
      ./configure --with-llvm=/path/to/llvm-3.4 \
                  --with-stp=/path/to/stp \
                  --with-uclibc=/path/to/klee-uclibc \
                  --enable-posix-runtime
    $ make ENABLE_OPTIMIZED=1

CASTAN analyzes network functions built on the DPDK framework.
Although CASTAN does not require any changes to DPDK itself to work (other than what is done in [castan-dpdk.h](include/castan/castan-dpdk.h)), if the NF uses any DPDK data structures you may need to compile parts of DPDK into LLVM bit-code for analysis.
If the NF uses DPDK only for sending and receiving messages, and is otherwise self-contained, this procedure is not necessary.
We have prepared a fork of the DPDK repository with scripts to facilitate such a build:
https://github.com/nal-epfl/castan-dpdk/


## Building the Cache Model

CASTAN uses a cache model to predict the performance of memory accesses.
The model is built using standard documented cache parameters ([ContentionSetCacheModel.h](include/castan/Internal/ContentionSetCacheModel.h), and learned contention sets which are loaded from a file.
The [contention set file for the Intel(R) Xeon(R) CPU E5-2667v2](examples/XeonE52667v2.dat.bz2) is included in this repo for convenience, and can be used once decompressed with bunzip2.

Generating new models is done with the [dpdk-probe-cache](examples/dpdk-probe-cache/), [process-contention-sets](examples/cache-effects/process-contention-sets.cpp), and [dpdk-check-cache](examples/dpdk-check-cache) tools.
dpdk-probe-cache generates a contention set file based on a single probe within a single 1GB huge page.
process-contention-sets processes files from multiple probes to find the contention sets that hold across multiple pages.
Finally, dpdk-check-cache validates the model within a single page and optionally filters out contention sets that no longer hold.


## Using CASTAN

To analyze an NF, it must first be built into LLVM bit-code.
The NFs implemented in examples/ already do this automatically when built with make.

CASTAN uses the following argument syntax:

    $ castan --max-loops=<n> \
             [--worst-case-sym-indices] \
             [--rainbow-table <rainbow-table-file>] \
             [--output-unreconciled] \
             [-max-memory=<n>] \
             <NF-bit-code-file>

Where the arguments mean:

 * --max-loops=<n>: The number of packets to generate.
 * --worst-case-sym-indices: Compute adversarial values for symbolic pointers.
 * --rainbow-table <rainbow-table-file>: Specify a rainbow table to use during havoc reconciliation.
 * --output-unreconciled: Enable outputting packets that have unreconciled havocs.
 * -max-memory: CASTAN may need a fair bit of memory to process some NFs and KLEE default to a 2GB cap which in some cases is not enough. This option can increase the cap by specifying a larger value in MB.
 * \<NF-bit-code-file\>: Specify the NF's LLVM bit-code.

CASTAN creates output files in the klee-last folder:

 * test*.ktest: Concretized adversarial inputs.
 * test*.cache: Report with predicted performance metrics.

KTEST files can be converted into PCAP files with the ktest2pcap tool:

    $ ktest2pcap <input-ktest-file> <output-pcap-file>

Many of the NFs in the examples directory have an additional make target that automates these steps:

    make castan

This generates nf.pcap with the adversarial workload.

Running this for the [LPM NF with a PATRICIA trie](examples/dpdk-lpm-btrie) looks like:

    $ docker start -ai castan                       # Start and attach to the container created earlier.
                                                    # The following commands run inside the container.
    $ cd ~/castan/examples
    $ bunzip2 XeonE52667v2.dat.bz2                  # Extract the default cache model.
    $ cd ~/castan/examples/dpdk-lpm-btrie
    $ make nf.bc                                    # Compile the NF into LLVM bit-code
    $ castan --max-loops=5 nf.bc                    # Press Ctrl-C once CASTAN starts outputting workloads.
                                                    # It will say: "Found path with 5 packets." after around 20 seconds.
    $ ktest2pcap klee-last/test000001.ktest nf.pcap # Generate PCAP file.
    $ sudo apt-get install tcpdump
    $ tcpdump -ner nf.pcap                          # Show workload.
      reading from file nf.pcap, link-type EN10MB (Ethernet)
      00:00:00.000000 00:00:00:00:00:00 > 00:00:00:00:00:00, ethertype IPv4 (0x0800), length 54: 0.0.0.0.0 > 1.1.1.0.0: UDP, length 0
      00:00:00.000000 00:00:00:00:00:00 > 00:00:00:00:00:00, ethertype IPv4 (0x0800), length 54: 0.0.0.0.0 > 3.1.1.1.0: UDP, length 0
      00:00:00.000000 00:00:00:00:00:00 > 00:00:00:00:00:00, ethertype IPv4 (0x0800), length 54: 0.0.0.0.0 > 5.1.1.0.0: UDP, length 0
      00:00:00.000000 00:00:00:00:00:00 > 00:00:00:00:00:00, ethertype IPv4 (0x0800), length 54: 0.0.0.0.0 > 2.1.1.0.0: UDP, length 0
      00:00:00.000000 00:00:00:00:00:00 > 00:00:00:00:00:00, ethertype IPv4 (0x0800), length 54: 0.0.0.0.0 > 0.0.0.0.0: UDP, length 0

Notice the first four packets are sent to IPs within the longer prefixes in the [routing table](examples/dpdk-lpm-btrie/testbed/routing-table.pfx2as).


## Measuring the Resulting Performance

The [CASTAN paper](https://dl.acm.org/citation.cfm?id=3230573) evaluates CASTAN by comparing the performance of several of the NFs in the examples directory under varying workloads, including the CASTAN generated one.
We automated the performance measurements using the scripts in the [scripts/perf/](scripts/perf/) directory.
The test-bed configuration is loaded from [config.sh](scripts/perf/config.sh).
We run the following command from the DUT to perform a single run for a single set of <NF, workload, metric>:

    bench.sh <NF> <thru-1p|latency> <workload>

Where the arguments are:
 * \<NF\>: the name of the NF to run, i.e. the subdirectory in examples where it resides.
 * <thru-1p|latency>: *thru-1p* performs a throughput experiment where packets are sent at varying throughputs to find the maximum point at which only 1% of packets are dropped. *latency* performs a latency experiment, where packets are sent one at a time and the latency is measured using hardware timestamps on the TG.
 * \<workload\>: The name of the pcap file to replay during the experiment.

Several workloads were used in the performance evaluation of the paper.
We include them in the [pcaps/](pcaps/) folder for convenience and reproducibility.
These include generic workloads used across all NFs and NF specific workloads.
The generic workloads also have special variants for load-balancer NFs that set the destination IP to the VIP, as described in the paper.


### Generic Workloads:

 * [1packet.pcap](pcaps/1packet.pcap) & [lb-1packet.pcap](pcaps/lb-1packet.pcap): A single packet.
 * [unirand.pcap](pcaps/unirand.pcap) & [lb-unirand.pcap](pcaps/lb-unirand.pcap): Packets following a uniform random distribution, like traditional adversarial traffic.
 * [zipf.pcap](pcaps/zipf.pcap) & [lb-zipf.pcap](pcaps/lb-zipf.pcap): Packets forming a Zipfian distribution, like typical Internet traffic.


### NF Specific Workloads:

| NF | CASTAN Workload | Manual Workload | Uniform Random with the same number of flows as CASTAN |
| --- | --- | --- | --- |
| NAT / Hash Table | [dpdk-nat-basichash-castan.pcap](pcaps/dpdk-nat-basichash-castan.pcap) | -- | [dpdk-nat-basichash-unirand-castan.pcap](pcaps/dpdk-nat-basichash-unirand-castan.pcap) |
| NAT / Hash Ring | [dpdk-nat-hashring-castan.pcap](pcaps/dpdk-nat-hashring-castan.pcap) | -- | [dpdk-nat-hashring-unirand-castan.pcap](pcaps/dpdk-nat-hashring-unirand-castan.pcap) |
| NAT / Red-Black Tree | [dpdk-nat-stlmap-castan.pcap](pcaps/dpdk-nat-stlmap-castan.pcap) | -- | [dpdk-nat-stlmap-unirand-castan.pcap](pcaps/dpdk-nat-stlmap-unirand-castan.pcap) |
| NAT / Unbalanced Tree | [dpdk-nat-tree-castan.pcap](pcaps/dpdk-nat-tree-castan.pcap) | [dpdk-nat-tree-manual.pcap](pcaps/dpdk-nat-tree-manual.pcap) | [dpdk-nat-tree-unirand-castan.pcap](pcaps/dpdk-nat-tree-unirand-castan.pcap) |
| LB  / Hash Table | [dpdk-lb-basichash-castan.pcap](pcaps/dpdk-lb-basichash-castan.pcap) | -- | [dpdk-lb-basichash-unirand-castan.pcap](pcaps/dpdk-lb-basichash-unirand-castan.pcap) |
| LB / Hash Ring | [dpdk-lb-hashring-castan.pcap](pcaps/dpdk-lb-hashring-castan.pcap) | -- | [dpdk-lb-hashring-unirand-castan.pcap](pcaps/dpdk-lb-hashring-unirand-castan.pcap) |
| LB / Red-Black Tree | [dpdk-lb-stlmap-castan.pcap](pcaps/dpdk-lb-stlmap-castan.pcap) | -- | [dpdk-lb-stlmap-unirand-castan.pcap](pcaps/dpdk-lb-stlmap-unirand-castan.pcap) |
| LB / Unbalanced Tree | [dpdk-lb-tree-castan.pcap](pcaps/dpdk-lb-tree-castan.pcap) | [dpdk-lb-tree-manual.pcap](pcaps/dpdk-lb-tree-manual.pcap) | [dpdk-lb-tree-unirand-castan.pcap](pcaps/dpdk-lb-tree-unirand-castan.pcap) |
| LPM / PATRICIA Trie | [dpdk-lpm-btrie-castan.pcap](pcaps/dpdk-lpm-btrie-castan.pcap) | [dpdk-lpm-btrie-manual.pcap](pcaps/dpdk-lpm-btrie-manual.pcap) | [dpdk-lpm-btrie-unirand-castan.pcap](pcaps/dpdk-lpm-btrie-unirand-castan.pcap) |
| LPM / 1-Stage Lookup | [dpdk-lpm-da-castan.pcap](pcaps/dpdk-lpm-da-castan.pcap) | -- | [dpdk-lpm-da-unirand-castan.pcap](pcaps/dpdk-lpm-da-unirand-castan.pcap) |
| LPM / 2-Stage Lookup (DPDK) | [dpdk-lpm-dpdklpm-castan.pcap](pcaps/dpdk-lpm-dpdklpm-castan.pcap) | -- | [dpdk-lpm-dpdklpm-unirand-castan.pcap](pcaps/dpdk-lpm-dpdklpm-unirand-castan.pcap) |
