from iocbuilder import ModuleBase, Device, IocDataStream
from iocbuilder.iocinit import quote_IOC_string
from iocbuilder.recordbase import Record
from iocbuilder.arginfo import *

class PvLogging(Device):
    LibFileList = ['pvlogging']
    DbdFileList = ['pvlogging']
    AutoInstantiate = True

    def __init__(self, access_file = None):
        self.__super.__init__()
        if access_file is None:
            self.access_file = self.ModuleFile('data/access.acf')
        else:
            self.access_file = access_file

    def InitialiseOnce(self):
        print 'asSetFilename %s' % quote_IOC_string(self.access_file)

    ArgInfo = makeArgInfo(__init__,
        access_file = Simple("Name of access control file"))

class BlacklistFile(Device):
    Dependencies = (PvLogging,)

    def __init__(self, blacklist):
        self.__super.__init__()
        self.blacklist = blacklist

    def Initialise(self):
        print 'load_logging_blacklist %s' % quote_IOC_string(self.blacklist)

class BlacklistPvs(BlacklistFile):
    AutoInstantiate = True      # Allow BlacklistPv to instantiate this

    Instance = None

    def __init__(self):
        self.__super.__init__(IocDataStream('auto_blacklist'))
        self.blacklist.write(' Automatically generated, do not edit\n')
        Record.AddMetadataHook(lambda _: None, Blacklist = self.add_blacklist)
        assert self.Instance is None, 'Cannot have multiple instances'
        BlacklistPvs.Instance = self

    def add_blacklist(self, record):
        self.blacklist_pv(record.name)

    @classmethod
    def blacklist_pv(cls, name):
        cls.Instance.blacklist.write('%s\n' % name)


class BlacklistPv(ModuleBase):
    '''Adds a single PV to the list of blacklisted PVs.'''

    Dependencies = (BlacklistPvs,)

    def __init__(self, name):
        BlacklistPvs.blacklist_pv(name)

    ArgInfo = makeArgInfo(__init__,
        name = Simple('PV to blacklist from PV logging'))
