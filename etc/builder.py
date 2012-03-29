from iocbuilder import Device, IocDataStream
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
    def __init__(self):
        self.__super.__init__(IocDataStream('auto_blacklist'))
        self.blacklist.write(' Automatically generated, do not edit\n')
        Record.AddMetadataHook(lambda _: None, Blacklist = self.add_blacklist)

    def add_blacklist(self, record):
        self.blacklist.write('%s\n' % record.name)

#    ArgInfo = makeArginfo(__init__)

#class BlacklistPv(Device):
#    def __init__(self, blacklistPvs, record):
#        self.__super.__init__()        
#        self.blacklistPvs.add_blacklist(record)
        
        
    
