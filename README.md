Heal translator for GlusterFS
=============================

Introduction
------------

The **heal** translator is a new feature for [GlusterFS](http://gluster.org) aimed to offer some support on healing operations to client side translators.

It is able to coordinate accesses to files being healed and arbitrate in cases when multiple clients are trying to heal a single file. It does not know how to heal a file but offers aids to the client side translators to simplify and improve the healing process. Using its features, it is possible to heal the data of a file without using locks on the file.


Important notes
---------------

**DO NOT USE IT IN PRODUCTION ENVIRONMENTS**

This translator is experimental and currently it is **only intended for developers**. The steps needed to get this translator running are quite complex for an average user without programming knowledge. And if/when things go wrong it may be difficult to understand what happened and how to solve the problem.

Once it matures, the installation and configuration will be much easier and it will be ready for being used in production environments like any other translator.


Requirements
------------

* Source code of GlusterFS 3.3.0 or later, configured, compiled and installed


Installation
------------

* Once the GlusterFS is configured, compiled and installed, edit the Makefile and modify the path to GlusterFS source code in variable *SRCDIR*.
* make
* make install

This should leave the translator module into the same place where GlusterFS has been installed.

The translator is compiled without any optimization for debugging purposes. 


Configuration
-------------

The vol file of each brick must be edited to add the heal translator at least just below the locks translator (it won't work as expected if it is not placed after the locks translator). However it's better to place it immediately on top of the posix translator.

For example, on a standard brick configuration:

    volume heal-test-posix
        type storage/posix
        option directory /bricks/b01
        option volume-id 135c4934-0039-4d8e-8f80-92383be99831
    end-volume

    volume heal-test-access-control
        type features/access-control
        subvolumes heal-test-posix
    end-volume

    volume heal-test-locks
        type features/locks
        subvolumes heal-test-access-control
    end-volume

The heal translator is added between the posix and access-control translators:

    volume heal-test-posix
        type storage/posix
        option directory /bricks/b01
        option volume-id 135c4934-0039-4d8e-8f80-92383be99831
    end-volume

    volume heal-test-heal
        type features/heal
        subvolumes heal-test-posix
    end-volume

    volume heal-test-access-control
        type features/access-control
        subvolumes heal-test-heal
    end-volume

    volume heal-test-locks
        type features/locks
        subvolumes heal-test-access-control
    end-volume


Technical information
---------------------

When one client detects an inconsistency and decides to heal a file, it sends a special request to the server protected by an inodelk/entrylk. Since the heal translator is placed below the locks translator, only one of these requests can arrive at any single moment. The first received heal request is allowed, and any subsequent request is denied until the first one finishes or the client disconnects. This guarantees that only one client will be sending heal requests.

Data heal requests can be sent without any locking bacause there would be only one client doing it. These requests can arrive concurrently with a normal write request. In these cases, the normal request takes precedence if the affected areas overlap. Once a fragment of the file has been written by a normal request after initiating the heal process, the translator ignores any healing data sent to one of the already updated areas.


Known problems
--------------

The heal translator is not fully implemented yet and does not allow normal writes to non-healed areas while a heal is in progress.

Code quality will need to be improved (some structural changes, code cleaning and adding documentation).

