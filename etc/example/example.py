import os

from pkg_resources import require
require('iocbuilder')

import iocbuilder
iocbuilder.ConfigureIOC(architecture = 'linux-x86_64')
from iocbuilder import ModuleVersion, records, WriteNamedIoc

TOP = os.path.realpath(os.path.join(os.path.dirname(__file__), '../..'))

ModuleVersion('pvlogging', home = TOP, use_name = False)

from iocbuilder.modules import pvlogging

pvlogging.PvLogging()
pvlogging.BlacklistPvs()

# A simple PV to test with
iocbuilder.SetDevice('TEST', 1, 'TS', 'XX')
records.ao('TEST')
records.ao('TEST2').Blacklist()

WriteNamedIoc(os.path.join(TOP, 'ioc'), 'TS-XX-IOC-99')
