#!/usr/bin/python

from __future__ import division
from struct import calcsize
import fcntl
import os

#
# Globals
#
DEVICE_PATH1 = '/dev/s19_devicess'
DEVICE_PATH2= '/dev/s19_devices'

#
# Utilities for calculating the IOCTL command codes.
#
sizeof = {
    'byte': calcsize('c'),
    'signed byte': calcsize('b'),
    'unsigned byte': calcsize('B'),
    'short': calcsize('h'),
    'unsigned short': calcsize('H'),
    'int': calcsize('i'),
    'unsigned int': calcsize('I'),
    'long': calcsize('l'),
    'unsigned long': calcsize('L'),
    'long long': calcsize('q'),
    'unsigned long long': calcsize('Q')
}

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRMASK = ((1 << _IOC_NRBITS)-1)
_IOC_TYPEMASK = ((1 << _IOC_TYPEBITS)-1)
_IOC_SIZEMASK = ((1 << _IOC_SIZEBITS)-1)
_IOC_DIRMASK = ((1 << _IOC_DIRBITS)-1)

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = (_IOC_NRSHIFT+_IOC_NRBITS)
_IOC_SIZESHIFT = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
_IOC_DIRSHIFT = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir, _type, nr, size):
    if type(_type) == str:
        _type = ord(_type)
        
    cmd_number = (((dir)  << _IOC_DIRSHIFT) | \
        ((_type) << _IOC_TYPESHIFT) | \
        ((nr)   << _IOC_NRSHIFT) | \
        ((size) << _IOC_SIZESHIFT))

    return cmd_number

def _IO(_type, nr):
    return _IOC(_IOC_NONE, _type, nr, 0)

def _IOR(_type, nr, size):
    return _IOC(_IOC_READ, _type, nr, sizeof[size])

def _IOW(_type, nr, size):
    return _IOC(_IOC_WRITE, _type, nr, sizeof[size])


def main():
    """Test the device driver"""
    os.system('rm -f /dev/s19_devic*')
    os.system('rmmod my_module')
    os.system('insmod /mnt/hgfs/shared_folder/OS_lab_ex_1/my_module.o')
    os.system('mknod %s c 254 3' % DEVICE_PATH1)
    
    #
    # Calculate the ioctl cmd number
    #
    MY_MAGIC = 'r'
    MY_OP1 = _IOW(MY_MAGIC, 0, 'int')
    
    #
    # Open the 'my_moulde' device driver
    #
    f = os.open(DEVICE_PATH1, os.O_RDWR)
    #g = os.open(DEVICE_PATH2, os.O_RDONLY)
    #os.write(g, 'hello world')
    #os.close(g)

    
   
    #
    # Test writing and reading
    #
    os.write(f, 'hello world whahtttata')
    print ('String read:\n%s') % os.read(f, 10)
    print ('String read:\n%s') % os.read(f, 5)
    os.write(f, 'hell of world whahtttata')
    print ('String read:\n%s') % os.read(f, 5)
    os.close(f)
    f = os.open(DEVICE_PATH1, os.O_RDWR)
    print ('String read:\n%s') % os.read(f, 90)
    #
    # Test the IOCTL command
    #
    # fcntl.ioctl(f, MY_OP1)    

    #
    # Finaly close the file
    #
   
    os.close(f)
    
if __name__ == '__main__':
    main()
    
