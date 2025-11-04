# SPDX-License-Identifier: AGPL-3.0-or-later

from SCons.Defaults import DefaultEnvironment

def PhonyTargets(env=None, **kw):
    if not env:
        env = DefaultEnvironment()
    for target, action in kw.items():
        if isinstance(action, list):
            action = ' '.join(action)
        env.AlwaysBuild(env.Alias(target, [], action))