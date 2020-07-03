import os

config = {
    #########################################################################
    ######## ANDROID GENERIC CONFIG KEYS/VAlUES

    'default_actions': [
        'build',
    ],
    'vcs_share_base': '/builds/hg-shared',
    #########################################################################


    #########################################################################
    'platform': 'gonk',
    'stage_platform': 'gonk',
    'enable_max_vsize': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/worker/tooltool-cache',
        'TOOLTOOL_HOME': '/builds',
        'LC_ALL': 'C',
        'PATH': '/usr/local/bin:/bin:/usr/bin',
        'SHIP_LICENSED_FONTS': '1',
    },
    'mozconfig_platform': 'gonk',
    'mozconfig_variant': 'nightly',
    # Boot2Gecko doesn't (yet) produce have a package file
    # from which to extract package metrics.
    'disable_package_metrics': True,
    #########################################################################
}
