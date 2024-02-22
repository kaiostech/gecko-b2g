# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from mozbuild.util import FileAvoidWrite

header_template = """#pragma GCC system_header
#pragma GCC visibility push(default)
{includes}
#pragma GCC visibility pop
"""

include_next_template = "#include_next <{header}>"


def undef_macros(header, *macro_list):
    push_str = ""
    pop_str = ""
    undef_str = ""
    for macro in macro_list:
        push_str += '#pragma push_macro("' + macro + '")\n'
        undef_str += "#undef " + macro + "\n"
        pop_str += '\n#pragma pop_macro("' + macro + '")'
    return push_str + undef_str + header + pop_str


# The 'unused' arg is the output file from the file_generate action. We actually
# generate all the files in header_list
def gen_wrappers(unused, outdir, *header_list):
    for header in header_list:
        with FileAvoidWrite(os.path.join(outdir, header)) as f:
            includes = include_next_template.format(header=header)
            if header == "wayland-util.h":
                # wayland-util.h in Wayland < 1.12 includes math.h inside an
                # extern "C" block, which breaks including the header from C++.
                # This was fixed in Wayland 1.12, but for versions earlier than
                # that, we work around that by force-including math.h first.
                includes = "#include <math.h>\n" + includes
            elif header == "wayland-client.h":
                # The system wayland-client.h uses quote includes for
                # wayland-util.h, which means wayland-util.h is picked from the
                # directory containing wayland-client.h first, and there's no
                # way around that with -I, -isystem, or other flags. So, we
                # force to include it from our wrapper, before including the
                # system header, so that our wayland-util.h wrapper is picked
                # first.
                includes = '#include "wayland-util.h"\n' + includes
            elif header.split("/")[-1] == "AudioSystem.h":
                # Enabling ACCESSIBILITY in Gecko causes name conflicts with
                # an enum in AudioStreamType.h on AOSP 13. To avoid compilation
                # errors, we undefine ACCESSIBILITY in the wrapper of
                # AudioSystem.h, which indirectly includes AudioStreamType.h.
                includes = undef_macros(includes, "ACCESSIBILITY")
            f.write(header_template.format(includes=includes))
