# HiBase
URL http://www.cs.hut.fi/~siva/hibase/



The HiBase Project
Helsinki University of Technology

Introduction

    In 1995 a three-year research project, the Shadows-project, was concluded at Helsinki University of Technology (HUT). The project studied and improved the shadow paging algorithm which is used in some database systems to provide crash recovery. Currently Shadows is being reimplemented commercially to be used as a basis for a transactional file system.

    In 1996 a development project, called HiBase (High-Performance Object Base Management System for Telecom Applications), was started as a joint effort mainly between HUT and Nokia Telecommunications. The aim of HiBase is to develop a persistent functional programming environment with challenging additional features required e.g. in telecom environments. These features include high performance, heterogeneous transaction load, complex dynamic database schemes, main-memory databases with real-time response, and very high availability ensured by replication and instantaneous takeover. 


Shades

    Early in 1996 a new recovery algorithm reminiscent of shadow paging was invented at HUT. The new algorithm, baptized Shades,* is particularly apt for versions (snapshots), small objects, and a main-memory environment.

    Avid development of Shades has furnished it with other desirable features, such as a real-time compacting garbage collector with negligible memory overhead. This suggests using Shades as a run-time of a persistent programming language.

    Depending on the update pattern, we have been able to achieve a sustained performance of 32,000 to 130,000 updates, or 30,000 TPC-B--like transactions per second with 50% memory utilization on a 200 MHz Pentium Pro.

    Except for some optimizations in garbage collection and IO, most of the implementation work on Shades has been done. Additional mechanisms for main-memory recovery and replication have been presented but not yet implemented. 

Indexes

    To date the following data structures have been implemented at HUT:

        LISP-like lists.
        Functional FIFO-queues.
        Priority queues based on binomial trees.
        Leaf-oriented AVL-trees.
        Path-compressed four-way tries.
        A functional string representation based on a variant of B+-trees. 

A Persistent Functional Programming Language

    The execution mechanism of the language is a bytecode interpreter. The run-time organization is based on continuation passing style (CPS), which is more expressive than that of e.g. Java TM.

    Since the byte code interpreter's internal data structures are managed by Shades, running bytecode processes can be treated with the same tools as data. For example, processes are recoverable, and they can be automatically resumed after recovery. Migrating processes, e.g. for purposes of load balancing, is as easy as migrating data.

    Currently HiBase can be interfaced to the outside world with TCP/IP connections, and in future also with OMG Interface Definition Language (IDL).

    Future tasks include designing the programming language, developing a compiler, and writing an extensive set of auxiliary programming tools. 

Administrativia

    M.Sc. Kenneth Oksanen has acted as the chief surgeon under the auspices of prof. Eljas Soisalon-Soininen. Oksanen will write his Ph.D. thesis on Shades and the byte code interpreter, Sirkku-Liisa Paajanen writes her M.Sc. thesis on index structures in Shades, and Kengatharan Sivalingam is finishing his M.Sc. thesis on versioning and the relational data model implementation of the preceding Shadows project. Antti-Pekka Liedes has participated as a half-time employee in the project, and Lars Wirzenius joined the HUT team in the beginning of '97.

    The planned project duration is three years. 40% of the HiBase-project is funded by TEKES. 

-------------------
* "The Shades is an ancient part of Ankh-Morpork considered considerably more unpleasant and disreputable than the rest of the city. This always amazes visitors." -- Terry Pratchett in Wyrd Sisters

siva@iki.fi


