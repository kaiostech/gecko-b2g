# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Source-test jobs can run on multiple platforms.  These transforms allow jobs
with either `platform` or a list of `platforms`, and set the appropriate
treeherder configuration and attributes for that platform.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.workertypes import worker_type_implementation


def disable_non_linux_workers(config, jobs):
    """
    Never try to run tasks on macosx or windows workers.
    """
    for job in jobs:
        impl, os = worker_type_implementation(config.graph_config, job['worker-type'])
        if os in ('macosx', 'windows'):
            job['optimization'] = {'always': None}
        yield job
