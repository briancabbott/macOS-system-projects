/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/assert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOFDiskPartitionScheme.h>
#include <libkern/OSByteOrder.h>

#define super IOPartitionScheme
OSDefineMetaClassAndStructors(IOFDiskPartitionScheme, IOPartitionScheme);

//
// Notes
//
// o the on-disk structure's fields are little-endian formatted
// o the relsect and numsect block values assume the drive's natural block size
// o the relsect block value is:
//   o for data partitions:
//     o relative to the FDisk map that defines the partition
//   o for extended partitions defined in the root-level FDisk map:
//     o relative to the FDisk map that defines the partition (start of disk)
//   o for extended partitions defined in a second-level or deeper FDisk map:
//     o relative to the second-level FDisk map, regardless of depth
// o the valid extended partition types are: 0x05, 0x0F, 0x85 
// o there should be no more than one extended partition defined per FDisk map
//

#define kIOFDiskPartitionSchemeContentTable "Content Table"

bool IOFDiskPartitionScheme::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    // State our assumptions.

    assert(sizeof(fdisk_part) ==  16);              // (compiler/platform check)
    assert(sizeof(disk_blk0)  == 512);              // (compiler/platform check)

    // Ask our superclass' opinion.

    if ( super::init(properties) == false )  return false;

    // Initialize our state.

    _partitions = 0;

    return true;
}

void IOFDiskPartitionScheme::free()
{
    //
    // Free all of this object's outstanding resources.
    //

    if ( _partitions )  _partitions->release();

    super::free();
}

IOService * IOFDiskPartitionScheme::probe(IOService * provider, SInt32 * score)
{
    //
    // Determine whether the provider media contains an FDisk partition map.
    //

    // State our assumptions.

    assert(OSDynamicCast(IOMedia, provider));

    // Ask our superclass' opinion.

    if ( super::probe(provider, score) == 0 )  return 0;

    // Scan the provider media for an FDisk partition map.

    _partitions = scan(score);

    // There might be an FDisk partition scheme on disk with boot code, but with
    // no partitions defined.  We don't consider this a match and return failure
    // from probe.

    if ( _partitions && _partitions->getCount() == 0 )
    {
        _partitions->release();
        _partitions = 0;        
    }

    return ( _partitions ) ? this : 0;
}

bool IOFDiskPartitionScheme::start(IOService * provider)
{
    //
    // Publish the new media objects which represent our partitions.
    //

    IOMedia *    partition;
    OSIterator * partitionIterator;

    // State our assumptions.

    assert(_partitions);

    // Ask our superclass' opinion.

    if ( super::start(provider) == false )  return false;

    // Attach and register the new media objects representing our partitions.

    partitionIterator = OSCollectionIterator::withCollection(_partitions);
    if ( partitionIterator == 0 )  return false;

    while ( (partition = (IOMedia *) partitionIterator->getNextObject()) )
    {
        if ( partition->attach(this) )
        {
            attachMediaObjectToDeviceTree(partition);

            partition->registerService();
        }
    }

    partitionIterator->release();

    // set partition scheme to be valid
    _partitionSchemeState |= kIOPartitionScheme_partition_valid;

    return true;
}

void IOFDiskPartitionScheme::stop(IOService * provider)
{
    //
    // Clean up after the media objects we published before terminating.
    //

    IOMedia *    partition;
    OSIterator * partitionIterator;

    // State our assumptions.

    assert(_partitions);

    // Detach the media objects we previously attached to the device tree.

    partitionIterator = OSCollectionIterator::withCollection(_partitions);

    if ( partitionIterator )
    {
        while ( (partition = (IOMedia *) partitionIterator->getNextObject()) )
        {
            detachMediaObjectFromDeviceTree(partition);
        }

        partitionIterator->release();
    }

    super::stop(provider);
}

IOReturn IOFDiskPartitionScheme::requestProbe(IOOptionBits options)
{
    //
    // Request that the provider media be re-scanned for partitions.
    //

    OSSet *         partitions    = 0;
    OSSet *         partitionsNew;
    OSIterator *    partitionLockingIterator;
    OSArray *       partitionsLocked;
    SInt32          score         = 0;

    // Scan the provider media for partitions.
    if ( ( _partitionSchemeState & kIOPartitionScheme_partition_valid ) == 0 )
    {
        return kIOReturnError;
    }

    partitionsNew = scan( &score );

    if ( partitionsNew )
    {

        if ( lockForArbitration( false ) )  // Attempt to lock ourself before attempting to lock the partitions
        {

            // Allocate an array to record which child partitions we have locked for arbitration
            partitionsLocked = OSArray::withCapacity( _partitions->getCount( ) );

            if ( partitionsLocked )
            {

                // Create an iterator over our current partiitons
                partitionLockingIterator = OSCollectionIterator::withCollection( _partitions );

                if ( partitionLockingIterator )
                {

                    do
                    {  // do-while to break out of if we fail to lock any children

                        // Attempt to lock each of the existing partitions before juxtaposing them
                        IOMedia *   partitionToLock;
                        bool        lockSuccess = false;

                        while ( ( partitionToLock = (IOMedia *) partitionLockingIterator->getNextObject( ) ) )
                        {
                            lockSuccess = partitionToLock->lockForArbitration( false );
                            if ( lockSuccess )
                            {
                                // Place successfully locked partitions in an OSArray so that we can iterate through
                                // and unlock them without risking an OSCollectionIterator failing to allocate
                                if ( !partitionsLocked->setObject( partitionToLock ) )
                                {
                                    // Our lock was successful, but we failed to record it, so we have to back out
                                    lockSuccess = false;
                                    partitionToLock->unlockForArbitration( );
                                    break;
                                }
                            }
                            else
                            {
                                break;  // We failed to lock this partition, skip other partitions
                            }
                        }
                        if ( !lockSuccess )
                        {
                            break;  // One of the partitions failed to lock, abort the juxtaposition and release the locks that we managed to get
                        }

                        // Everything was successully locked, continue to rearrage the partitions
                        partitions = juxtaposeMediaObjects( _partitions, partitionsNew );

                        if ( partitions )
                        {
                            _partitions->release( );

                            _partitions = partitions;
                        }

                    } while ( 0 );  // Break to here if we fail to lock any child partitions

                    partitionLockingIterator->release( );

                    // Unlock only the partitions which we were able to lock successfully in the reverse order of how we acquired them
                    IOMedia *   partitionToUnlock;
                    unsigned int lockedParitionsCount;

                    while ( ( lockedParitionsCount = partitionsLocked->getCount( ) ) > 0 )
                    {

                        partitionToUnlock = (IOMedia *) partitionsLocked->getObject( lockedParitionsCount - 1 );
                        partitionToUnlock->unlockForArbitration( );
                        partitionsLocked->removeObject( lockedParitionsCount - 1 );

                    }

                }

                partitionsLocked->release( );

            }

            unlockForArbitration( );

        }

        partitionsNew->release( );

    }

    return partitions ? kIOReturnSuccess : kIOReturnError;
}

OSSet * IOFDiskPartitionScheme::scan(SInt32 * score)
{
    //
    // Scan the provider media for an FDisk partition map.  Returns the set
    // of media objects representing each of the partitions (the retain for
    // the set is passed to the caller), or null should no partition map be
    // found.  The default probe score can be adjusted up or down, based on
    // the confidence of the scan.
    //

    IOBufferMemoryDescriptor * buffer         = 0;
    IOByteCount                bufferSize     = 0;
    UInt32                     fdiskBlock     = 0;
    UInt32                     fdiskBlockExtn = 0;
    UInt32                     fdiskBlockNext = 0;
    UInt32                     fdiskID        = 0;
    disk_blk0 *                fdiskMap       = 0;
    IOMedia *                  media          = getProvider();
    UInt64                     mediaBlockSize = media->getPreferredBlockSize();
    bool                       mediaIsOpen    = false;
    OSSet *                    partitions     = 0;
    IOReturn                   status         = kIOReturnError;

    // Determine whether this media is formatted.

    if ( media->isFormatted() == false )  goto scanErr;

    // Determine whether this media has an appropriate block size.

    if ( (mediaBlockSize % sizeof(disk_blk0)) )  goto scanErr;

    // Allocate a buffer large enough to hold one map, rounded to a media block.

    bufferSize = IORound(sizeof(disk_blk0), mediaBlockSize);
    buffer     = IOBufferMemoryDescriptor::withCapacity(
                                           /* capacity      */ bufferSize,
                                           /* withDirection */ kIODirectionIn );
    if ( buffer == 0 )  goto scanErr;

    // Allocate a set to hold the set of media objects representing partitions.

    partitions = OSSet::withCapacity(4);
    if ( partitions == 0 )  goto scanErr;

    // Open the media with read access.

    mediaIsOpen = open(this, 0, kIOStorageAccessReader);
    if ( mediaIsOpen == false )  goto scanErr;

    // Scan the media for FDisk partition map(s).

    do
    {
        // Read the next FDisk map into our buffer.

        status = media->read(this, fdiskBlock * mediaBlockSize, buffer);
        if ( status != kIOReturnSuccess )  goto scanErr;

        fdiskMap = (disk_blk0 *) buffer->getBytesNoCopy();

        // Determine whether the partition map signature is present.

        if ( OSSwapLittleToHostInt16(fdiskMap->signature) != DISK_SIGNATURE )
        {
            goto scanErr;
        }

        // Scan for valid partition entries in the partition map.

        fdiskBlockNext = 0;

        for ( unsigned index = 0; index < DISK_NPART; index++ )
        {
            // Determine whether this is an extended (vs. data) partition.

            if ( isPartitionExtended(fdiskMap->parts + index) )    // (extended)
            {
                // If peer extended partitions exist, we accept only the first.

                if ( fdiskBlockNext == 0 )      // (no peer extended partition)
                {
                    fdiskBlockNext = fdiskBlockExtn +
                                     OSSwapLittleToHostInt32(
                                    /* data */ fdiskMap->parts[index].relsect );

                    if ( fdiskBlockNext * mediaBlockSize >= media->getSize() )
                    {
                        fdiskBlockNext = 0;       // (exceeds confines of media)
                    }
                }
            }
            else if ( isPartitionUsed(fdiskMap->parts + index) )       // (data)
            {
                // Prepare this partition's ID.

                fdiskID = ( fdiskBlock == 0 ) ? (index + 1) : (fdiskID + 1);

                // Determine whether the partition is corrupt (fatal).

                if ( isPartitionCorrupt(
                                   /* partition   */ fdiskMap->parts + index,
                                   /* partitionID */ fdiskID,
                                   /* fdiskBlock  */ fdiskBlock ) )
                {
                    goto scanErr;
                }

                // Determine whether the partition is invalid (skipped).

                if ( isPartitionInvalid(
                                   /* partition   */ fdiskMap->parts + index,
                                   /* partitionID */ fdiskID,
                                   /* fdiskBlock  */ fdiskBlock ) )
                {
                    continue;
                }

                // Create a media object to represent this partition.

                IOMedia * newMedia = instantiateMediaObject(
                                   /* partition   */ fdiskMap->parts + index,
                                   /* partitionID */ fdiskID,
                                   /* fdiskBlock  */ fdiskBlock );

                if ( newMedia )
                {
                    partitions->setObject(newMedia);
                    newMedia->release();
                }
            }
        }

        // Prepare for first extended partition, if any.

        if ( fdiskBlock == 0 )
        {
            fdiskID        = DISK_NPART;
            fdiskBlockExtn = fdiskBlockNext;
        }

    } while ( (fdiskBlock = fdiskBlockNext) );

    // Release our resources.

    close(this);
    buffer->release();

    return partitions;

scanErr:

    // Release our resources.

    if ( mediaIsOpen )  close(this);
    if ( partitions )  partitions->release();
    if ( buffer )  buffer->release();

    return 0;
}

bool IOFDiskPartitionScheme::isPartitionExtended(fdisk_part * partition)
{
    //
    // Ask whether the given partition is extended.
    //

    return ( partition->systid == 0x05 ||
             partition->systid == 0x0F ||
             partition->systid == 0x85 );
}

bool IOFDiskPartitionScheme::isPartitionUsed(fdisk_part * partition)
{
    //
    // Ask whether the given partition is used.
    //

    return ( partition->systid != 0 && partition->numsect != 0 );
}

bool IOFDiskPartitionScheme::isPartitionCorrupt( fdisk_part * partition,
                                                 UInt32       partitionID,
                                                 UInt32       fdiskBlock )
{
    //
    // Ask whether the given partition appears to be corrupt.  A partition that
    // is corrupt will cause the failure of the FDisk partition map recognition
    // altogether.
    //

    // Determine whether the boot indicator is valid.

    if ( (partition->bootid & 0x7F) )  return true;

    return false;
}

bool IOFDiskPartitionScheme::isPartitionInvalid( fdisk_part * partition,
                                                 UInt32       partitionID,
                                                 UInt32       fdiskBlock )
{
    //
    // Ask whether the given partition appears to be invalid.  A partition that
    // is invalid will cause it to be skipped in the scan, but will not cause a
    // failure of the FDisk partition map recognition.
    //

    IOMedia * media          = getProvider();
    UInt64    mediaBlockSize = media->getPreferredBlockSize();
    UInt64    partitionBase  = 0;
    UInt64    partitionSize  = 0;

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapLittleToHostInt32(partition->relsect) + fdiskBlock;
    partitionSize  = OSSwapLittleToHostInt32(partition->numsect);
    partitionBase *= mediaBlockSize;
    partitionSize *= mediaBlockSize;

    // Determine whether the partition shares space with the partition map.

    if ( partitionBase == fdiskBlock * mediaBlockSize )  return true;

    // Determine whether the partition starts at (or past) the end-of-media.

    if ( partitionBase >= media->getSize() )  return true;

    return false;
}

IOMedia * IOFDiskPartitionScheme::instantiateMediaObject(
                                                     fdisk_part * partition,
                                                     UInt32       partitionID,
                                                     UInt32       fdiskBlock )
{
    //
    // Instantiate a new media object to represent the given partition.
    //

    IOMedia * media          = getProvider();
    UInt64    mediaBlockSize = media->getPreferredBlockSize();
    UInt64    partitionBase  = 0;
    char *    partitionHint  = 0;
    UInt64    partitionSize  = 0;

    // Compute the relative byte position and size of the new partition.

    partitionBase  = OSSwapLittleToHostInt32(partition->relsect) + fdiskBlock;
    partitionSize  = OSSwapLittleToHostInt32(partition->numsect);
    partitionBase *= mediaBlockSize;
    partitionSize *= mediaBlockSize;

    // Clip the size of the new partition if it extends past the end-of-media.

    if ( partitionBase + partitionSize > media->getSize() )
    {
        partitionSize = media->getSize() - partitionBase;
    }

    // Look up a type for the new partition.

    char hintIndex[5];

    snprintf(hintIndex, sizeof(hintIndex), "0x%02X", partition->systid & 0xFF);

    partitionHint = hintIndex;

    OSDictionary * hintTable = OSDynamicCast( 
              /* type     */ OSDictionary,
              /* instance */ getProperty(kIOFDiskPartitionSchemeContentTable) );

    if ( hintTable )
    {
        OSString * hintValue;

        hintValue = OSDynamicCast(OSString, hintTable->getObject(hintIndex));

        if ( hintValue ) partitionHint = (char *) hintValue->getCStringNoCopy();
    }

    // Create the new media object.

    IOMedia * newMedia = instantiateDesiredMediaObject(
                                   /* partition   */ partition,
                                   /* partitionID */ partitionID,
                                   /* fdiskBlock  */ fdiskBlock );

    if ( newMedia )
    {
         if ( newMedia->init(
                /* base               */ partitionBase,
                /* size               */ partitionSize,
                /* preferredBlockSize */ mediaBlockSize,
                /* attributes         */ media->getAttributes(),
                /* isWhole            */ false,
                /* isWritable         */ media->isWritable(),
                /* contentHint        */ partitionHint ) )
        {
            // Set a name for this partition.

            char name[24];
            snprintf(name, sizeof(name), "Untitled %d", (int) partitionID);
            newMedia->setName(name);

            // Set a location value (the partition number) for this partition.

            char location[12];
            snprintf(location, sizeof(location), "%d", (int) partitionID);
            newMedia->setLocation(location);

            // Set the "Base" key for this partition.

            newMedia->setProperty(kIOMediaBaseKey, partitionBase, 64);

            // Set the "Partition ID" key for this partition.

            newMedia->setProperty(kIOMediaPartitionIDKey, partitionID, 32);
        }
        else
        {
            newMedia->release();
            newMedia = 0;
        }
    }

    return newMedia;
}

IOMedia * IOFDiskPartitionScheme::instantiateDesiredMediaObject(
                                                     fdisk_part * partition,
                                                     UInt32       partitionID,
                                                     UInt32       fdiskBlock )
{
    //
    // Allocate a new media object (called from instantiateMediaObject).
    //

    return new IOMedia;
}

OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  0);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  1);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  2);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  3);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  4);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  5);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  6);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  7);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  8);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme,  9);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 10);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 11);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 12);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 13);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 14);
OSMetaClassDefineReservedUnused(IOFDiskPartitionScheme, 15);
