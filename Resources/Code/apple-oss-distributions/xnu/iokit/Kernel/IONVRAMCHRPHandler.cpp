/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#define NVRAM_CHRP_APPLE_HEADER_NAME_V1  "nvram"
#define NVRAM_CHRP_APPLE_HEADER_NAME_V2  "2nvram"

#define NVRAM_CHRP_PARTITION_NAME_COMMON_V1   "common"
#define NVRAM_CHRP_PARTITION_NAME_SYSTEM_V1   "system"
#define NVRAM_CHRP_PARTITION_NAME_COMMON_V2   "2common"
#define NVRAM_CHRP_PARTITION_NAME_SYSTEM_V2   "2system"

#define NVRAM_CHRP_LENGTH_BLOCK_SIZE 0x10 // CHRP length field is in 16 byte blocks

typedef struct chrp_nvram_header { //16 bytes
	uint8_t  sig;
	uint8_t  cksum; // checksum on sig, len, and name
	uint16_t len;   // total length of the partition in 16 byte blocks starting with the signature
	// and ending with the last byte of data area, ie len includes its own header size
	char     name[12];
	uint8_t  data[0];
} chrp_nvram_header_t;

typedef struct apple_nvram_header {  // 16 + 16 bytes
	struct   chrp_nvram_header chrp;
	uint32_t adler;
	uint32_t generation;
	uint8_t  padding[8];
} apple_nvram_header_t;

typedef struct {
	NVRAMPartitionType        type;
	uint32_t                    offset;
	uint32_t                    size;
	OSSharedPtr<OSDictionary> &dict;
} NVRAMRegionInfo;

class IONVRAMCHRPHandler : public IODTNVRAMFormatHandler
{
private:
	bool              _newData;
	IONVRAMController *_nvramController;
	IODTNVRAM         *_provider;
	NVRAMVersion      _version;
	uint32_t          _generation;

	uint8_t           *_nvramImage;
	uint32_t          _nvramSize;

	uint32_t          _commonPartitionOffset;
	uint32_t          _commonPartitionSize;

	uint32_t          _systemPartitionOffset;
	uint32_t          _systemPartitionSize;

	OSSharedPtr<OSDictionary>    &_commonDict;
	OSSharedPtr<OSDictionary>    &_systemDict;

	uint32_t          _commonUsed;
	uint32_t          _systemUsed;

	IOReturn unserializeImage(const uint8_t *image, IOByteCount length);
	IOReturn unserializeVariables();

	IOReturn serializeVariables(void);

	static OSSharedPtr<OSData> unescapeBytesToData(const uint8_t *bytes, uint32_t length);
	static OSSharedPtr<OSData> escapeDataToData(OSData * value);

	static bool convertPropToObject(const uint8_t *propName, uint32_t propNameLength, const uint8_t *propData, uint32_t propDataLength,
	    LIBKERN_RETURNS_RETAINED const OSSymbol **propSymbol, LIBKERN_RETURNS_RETAINED OSObject **propObject);
	static bool convertPropToObject(const uint8_t *propName, uint32_t propNameLength, const uint8_t *propData, uint32_t propDataLength,
	    OSSharedPtr<const OSSymbol>& propSymbol, OSSharedPtr<OSObject>& propObject);
	static bool convertObjectToProp(uint8_t *buffer, uint32_t *length, const OSSymbol *propSymbol, OSObject *propObject);
	static bool convertObjectToProp(uint8_t *buffer, uint32_t *length, const char *propSymbol, OSObject *propObject);

public:
	virtual
	~IONVRAMCHRPHandler() APPLE_KEXT_OVERRIDE;
	IONVRAMCHRPHandler(OSSharedPtr<OSDictionary> &commonDict, OSSharedPtr<OSDictionary> &systemDict);

	static bool isValidImage(const uint8_t *image, IOByteCount length);

	static  IONVRAMCHRPHandler *init(IODTNVRAM *provider, const uint8_t *image, IOByteCount length,
	    OSSharedPtr<OSDictionary> &commonDict, OSSharedPtr<OSDictionary> &systemDict);

	virtual IOReturn setVariable(const uuid_t *varGuid, const char *variableName, OSObject *object) APPLE_KEXT_OVERRIDE;
	virtual bool setController(IONVRAMController *controller) APPLE_KEXT_OVERRIDE;
	virtual bool sync(void) APPLE_KEXT_OVERRIDE;
	virtual uint32_t getGeneration(void) const APPLE_KEXT_OVERRIDE;
	virtual uint32_t getVersion(void) const APPLE_KEXT_OVERRIDE;
	virtual uint32_t getSystemUsed(void) const APPLE_KEXT_OVERRIDE;
	virtual uint32_t getCommonUsed(void) const APPLE_KEXT_OVERRIDE;
};

static const char *
get_bank_version_string(int version)
{
	switch (version) {
	case kNVRAMVersion1:
		return NVRAM_CHRP_APPLE_HEADER_NAME_V1;
	case kNVRAMVersion2:
		return NVRAM_CHRP_APPLE_HEADER_NAME_V2;
	default:
		return "Unknown";
	}
}

static uint32_t
adler32(const uint8_t *buffer, size_t length)
{
	uint32_t offset;
	uint32_t adler, lowHalf, highHalf;

	lowHalf = 1;
	highHalf = 0;

	for (offset = 0; offset < length; offset++) {
		if ((offset % 5000) == 0) {
			lowHalf  %= 65521L;
			highHalf %= 65521L;
		}

		lowHalf += buffer[offset];
		highHalf += lowHalf;
	}

	lowHalf  %= 65521L;
	highHalf %= 65521L;

	adler = (highHalf << 16) | lowHalf;

	return adler;
}

static uint32_t
nvram_get_adler(uint8_t *buf, int version)
{
	return ((struct apple_nvram_header *)buf)->adler;
}

static uint32_t
adler32_with_version(const uint8_t *buf, size_t len, int version)
{
	size_t offset;

	switch (version) {
	case kNVRAMVersion1:
	case kNVRAMVersion2:
		offset = offsetof(struct apple_nvram_header, generation);
		break;
	default:
		return 0;
	}

	return adler32(buf + offset, len - offset);
}

static uint8_t
chrp_checksum(const struct chrp_nvram_header *hdr)
{
	uint16_t      sum;
	const uint8_t *p;
	const uint8_t *begin = (const uint8_t *)hdr + offsetof(struct chrp_nvram_header, len);
	const uint8_t *end = (const uint8_t *)hdr + offsetof(struct chrp_nvram_header, data);

	// checksum the header (minus the checksum itself)
	sum = hdr->sig;
	for (p = begin; p < end; p++) {
		sum += *p;
	}
	while (sum > 0xff) {
		sum = (sum & 0xff) + (sum >> 8);
	}

	return sum & 0xff;
}

static IOReturn
nvram_validate_header_v1v2(const uint8_t * buf, uint32_t *generation, int version)
{
	IOReturn   result = kIOReturnError;
	uint8_t    checksum;
	const char *header_string = get_bank_version_string(version);
	struct     chrp_nvram_header *chrp_header = (struct chrp_nvram_header *)buf;
	uint32_t   local_gen = 0;

	require(buf != nullptr, exit);

	// <rdar://problem/73454488> Recovery Mode [Internal Build] 18D52-->18E141 [J307/308 Only]
	// we can only compare the first "nvram" parts of the name as some devices have additional junk from
	// a previous build likely copying past bounds of the "nvram" name in the const section
	if (memcmp(header_string, chrp_header->name, strlen(header_string)) == 0) {
		checksum = chrp_checksum(chrp_header);
		if (checksum == chrp_header->cksum) {
			result = kIOReturnSuccess;
			local_gen = ((struct apple_nvram_header*)buf)->generation;

			DEBUG_INFO("Found %s gen=%u\n", header_string, local_gen);

			if (generation) {
				*generation = local_gen;
			}
		} else {
			DEBUG_INFO("invalid checksum in header, found %#02x, expected %#02x\n", chrp_header->cksum, checksum);
		}
	} else {
		DEBUG_INFO("invalid bank for \"%s\", name = %#02x %#02x %#02x %#02x\n", header_string,
		    chrp_header->name[0],
		    chrp_header->name[1],
		    chrp_header->name[2],
		    chrp_header->name[3]);
	}

exit:
	return result;
}

static void
nvram_set_apple_header(uint8_t *buf, size_t len, uint32_t generation, int version)
{
	if (version == kNVRAMVersion1 ||
	    version == kNVRAMVersion2) {
		struct apple_nvram_header *apple_hdr = (struct apple_nvram_header *)buf;
		generation += 1;
		apple_hdr->generation = generation;
		apple_hdr->adler = adler32_with_version(buf, len, version);
	}
}

static NVRAMVersion
validateNVRAMVersion(const uint8_t *buf, size_t len, uint32_t *generation)
{
	NVRAMVersion version = kNVRAMVersionUnknown;

	if (nvram_validate_header_v1v2(buf, generation, kNVRAMVersion1) == kIOReturnSuccess) {
		version = kNVRAMVersion1;
		goto exit;
	}

	if (nvram_validate_header_v1v2(buf, generation, kNVRAMVersion2) == kIOReturnSuccess) {
		version = kNVRAMVersion2;
		goto exit;
	}

	DEBUG_INFO("Unable to determine version\n");

exit:
	DEBUG_INFO("version=%u\n", version);
	return version;
}

IONVRAMCHRPHandler::~IONVRAMCHRPHandler()
{
}

bool
IONVRAMCHRPHandler::isValidImage(const uint8_t *image, IOByteCount length)
{
	return validateNVRAMVersion(image, length, nullptr) != kNVRAMVersionUnknown;
}

IONVRAMCHRPHandler*
IONVRAMCHRPHandler::init(IODTNVRAM *provider, const uint8_t *image, IOByteCount length,
    OSSharedPtr<OSDictionary> &commonDict, OSSharedPtr<OSDictionary> &systemDict)
{
	IONVRAMCHRPHandler *handler = new IONVRAMCHRPHandler(commonDict, systemDict);

	handler->_provider = provider;

	if ((image != nullptr) && (length != 0)) {
		if (handler->unserializeImage(image, length) != kIOReturnSuccess) {
			DEBUG_ALWAYS("Unable to unserialize image, len=%#x\n", (unsigned int)length);
		}
	}

	return handler;
}

IONVRAMCHRPHandler::IONVRAMCHRPHandler(OSSharedPtr<OSDictionary> &commonDict, OSSharedPtr<OSDictionary> &systemDict) :
	_commonDict(commonDict),
	_systemDict(systemDict)
{
}

IOReturn
IONVRAMCHRPHandler::unserializeImage(const uint8_t *image, IOByteCount length)
{
	uint32_t partitionOffset, partitionLength;
	uint32_t currentLength, currentOffset = 0;
	uint32_t hdr_adler, calculated_adler;

	_commonPartitionOffset = 0xFFFFFFFF;
	_systemPartitionOffset = 0xFFFFFFFF;

	_version = validateNVRAMVersion(image, _nvramSize, &_generation);
	require(_version != kNVRAMVersionUnknown, exit);

	if (_nvramImage) {
		IOFreeData(_nvramImage, _nvramSize);
	}

	_nvramImage = IONewData(uint8_t, length);
	_nvramSize = (uint32_t)length;
	bcopy(image, _nvramImage, _nvramSize);

	hdr_adler = nvram_get_adler(_nvramImage, _version);
	calculated_adler = adler32_with_version(_nvramImage, _nvramSize, _version);

	if (hdr_adler != calculated_adler) {
		panic("header adler %#08X != calculated_adler %#08X\n", hdr_adler, calculated_adler);
	}

	// Look through the partitions to find the common and system partitions.
	while (currentOffset < _nvramSize) {
		bool common_partition;
		bool system_partition;
		const chrp_nvram_header_t * header = (chrp_nvram_header_t *)(_nvramImage + currentOffset);
		const uint8_t common_v1_name[sizeof(header->name)] = {NVRAM_CHRP_PARTITION_NAME_COMMON_V1};
		const uint8_t common_v2_name[sizeof(header->name)] = {NVRAM_CHRP_PARTITION_NAME_COMMON_V2};
		const uint8_t system_v1_name[sizeof(header->name)] = {NVRAM_CHRP_PARTITION_NAME_SYSTEM_V1};
		const uint8_t system_v2_name[sizeof(header->name)] = {NVRAM_CHRP_PARTITION_NAME_SYSTEM_V2};

		currentLength = header->len * NVRAM_CHRP_LENGTH_BLOCK_SIZE;

		if (currentLength < sizeof(chrp_nvram_header_t)) {
			break;
		}

		partitionOffset = currentOffset + sizeof(chrp_nvram_header_t);
		partitionLength = currentLength - sizeof(chrp_nvram_header_t);

		if ((partitionOffset + partitionLength) > _nvramSize) {
			break;
		}

		common_partition = (memcmp(header->name, common_v1_name, sizeof(header->name)) == 0) ||
		    (memcmp(header->name, common_v2_name, sizeof(header->name)) == 0);
		system_partition = (memcmp(header->name, system_v1_name, sizeof(header->name)) == 0) ||
		    (memcmp(header->name, system_v2_name, sizeof(header->name)) == 0);

		if (common_partition) {
			_commonPartitionOffset = partitionOffset;
			_commonPartitionSize = partitionLength;
		} else if (system_partition) {
			_systemPartitionOffset = partitionOffset;
			_systemPartitionSize = partitionLength;
		}
		currentOffset += currentLength;
	}

exit:
	DEBUG_ALWAYS("NVRAM : commonPartitionOffset - %#x, commonPartitionSize - %#x, systemPartitionOffset - %#x, systemPartitionSize - %#x\n",
	    _commonPartitionOffset, _commonPartitionSize, _systemPartitionOffset, _systemPartitionSize);

	return unserializeVariables();
}

IOReturn
IONVRAMCHRPHandler::unserializeVariables(void)
{
	uint32_t                    cnt, cntStart;
	const uint8_t               *propName, *propData;
	uint32_t                    propNameLength, propDataLength, regionIndex;
	OSSharedPtr<const OSSymbol> propSymbol;
	OSSharedPtr<OSObject>       propObject;
	NVRAMRegionInfo             *currentRegion;
	NVRAMRegionInfo             variableRegions[] = { { kIONVRAMPartitionCommon, _commonPartitionOffset, _commonPartitionSize, _commonDict},
							  { kIONVRAMPartitionSystem, _systemPartitionOffset, _systemPartitionSize, _systemDict} };

	DEBUG_INFO("...\n");

	for (regionIndex = 0; regionIndex < ARRAY_SIZE(variableRegions); regionIndex++) {
		currentRegion = &variableRegions[regionIndex];
		const uint8_t * imageData = _nvramImage + currentRegion->offset;

		if (currentRegion->size == 0) {
			continue;
		}

		currentRegion->dict = OSDictionary::withCapacity(1);

		DEBUG_INFO("region = %d\n", currentRegion->type);
		cnt = 0;
		while (cnt < currentRegion->size) {
			cntStart = cnt;
			// Break if there is no name.
			if (imageData[cnt] == '\0') {
				break;
			}

			// Find the length of the name.
			propName = imageData + cnt;
			for (propNameLength = 0; (cnt + propNameLength) < currentRegion->size;
			    propNameLength++) {
				if (imageData[cnt + propNameLength] == '=') {
					break;
				}
			}

			// Break if the name goes past the end of the partition.
			if ((cnt + propNameLength) >= currentRegion->size) {
				break;
			}
			cnt += propNameLength + 1;

			propData = imageData + cnt;
			for (propDataLength = 0; (cnt + propDataLength) < currentRegion->size;
			    propDataLength++) {
				if (imageData[cnt + propDataLength] == '\0') {
					break;
				}
			}

			// Break if the data goes past the end of the partition.
			if ((cnt + propDataLength) >= currentRegion->size) {
				break;
			}
			cnt += propDataLength + 1;

			if (convertPropToObject(propName, propNameLength,
			    propData, propDataLength,
			    propSymbol, propObject)) {
				DEBUG_INFO("adding %s, dataLength=%u\n", propSymbol.get()->getCStringNoCopy(), propDataLength);
				currentRegion->dict.get()->setObject(propSymbol.get(), propObject.get());
				if (_provider->_diags) {
					_provider->_diags->logVariable(_provider->getDictionaryType(currentRegion->dict.get()),
					    kIONVRAMOperationInit, propSymbol.get()->getCStringNoCopy(),
					    (void *)(uintptr_t)(cnt - cntStart));
				}
			}
		}

		if (currentRegion->type == kIONVRAMPartitionSystem) {
			_systemUsed = cnt;
		} else if (currentRegion->type == kIONVRAMPartitionCommon) {
			_commonUsed = cnt;
		}
	}

	if (_provider->_diags) {
		OSSharedPtr<OSNumber> val = OSNumber::withNumber(getSystemUsed(), 32);
		_provider->_diags->setProperty(kNVRAMSystemUsedKey, val.get());
		DEBUG_INFO("%s=%u\n", kNVRAMSystemUsedKey, getSystemUsed());

		val = OSNumber::withNumber(getCommonUsed(), 32);
		_provider->_diags->setProperty(kNVRAMCommonUsedKey, val.get());
		DEBUG_INFO("%s=%u\n", kNVRAMCommonUsedKey, getCommonUsed());
	}

	// Create the boot-args property if it is not in the dictionary.
	if (_systemDict != nullptr) {
		if (_systemDict->getObject(kIONVRAMBootArgsKey) == nullptr) {
			propObject = OSString::withCStringNoCopy("");
			if (propObject != nullptr) {
				_systemDict->setObject(kIONVRAMBootArgsKey, propObject.get());
			}
		}
	} else if (_commonDict != nullptr) {
		if (_commonDict->getObject(kIONVRAMBootArgsKey) == nullptr) {
			propObject = OSString::withCStringNoCopy("");
			if (propObject != nullptr) {
				_commonDict->setObject(kIONVRAMBootArgsKey, propObject.get());
			}
		}
	}

	_newData = true;

	DEBUG_INFO("%s _commonDict=%p _systemDict=%p\n", __FUNCTION__, _commonDict ? _commonDict.get() : nullptr, _systemDict ? _systemDict.get() : nullptr);

	return kIOReturnSuccess;
}

IOReturn
IONVRAMCHRPHandler::serializeVariables(void)
{
	IOReturn                          ret;
	bool                              ok = false;
	uint32_t                          length, maxLength, regionIndex;
	uint8_t                           *buffer, *tmpBuffer;
	const OSSymbol                    *tmpSymbol;
	OSObject                          *tmpObject;
	OSSharedPtr<OSCollectionIterator> iter;
	OSSharedPtr<OSNumber>             generation;
	uint8_t                           *nvramImage;
	NVRAMRegionInfo                   *currentRegion;
	NVRAMRegionInfo                   variableRegions[] = { { kIONVRAMPartitionCommon, _commonPartitionOffset, _commonPartitionSize, _commonDict},
								{ kIONVRAMPartitionSystem, _systemPartitionOffset, _systemPartitionSize, _systemDict} };

	require_action(_nvramController != nullptr, exit, (ret = kIOReturnNotReady, DEBUG_ERROR("No _nvramController\n")));
	require_action(_newData == true, exit, (ret = kIOReturnSuccess, DEBUG_INFO("No _newData to sync\n")));
	require_action(_nvramSize != 0, exit, (ret = kIOReturnSuccess, DEBUG_INFO("No nvram size info\n")));
	require_action(_nvramImage != nullptr, exit, (ret = kIOReturnSuccess, DEBUG_INFO("No nvram image info\n")));

	nvramImage = IONewZeroData(uint8_t, _nvramSize);
	require_action(nvramImage != nullptr, exit, (ret = kIOReturnNoMemory, DEBUG_ERROR("Can't create NVRAM image copy\n")));

	DEBUG_INFO("...\n");

	bcopy(_nvramImage, nvramImage, _nvramSize);

	for (regionIndex = 0; regionIndex < ARRAY_SIZE(variableRegions); regionIndex++) {
		currentRegion = &variableRegions[regionIndex];

		if (currentRegion->size == 0) {
			continue;
		}

		DEBUG_INFO("region = %d\n", currentRegion->type);
		buffer = tmpBuffer = nvramImage + currentRegion->offset;

		bzero(buffer, currentRegion->size);

		ok = true;
		maxLength = currentRegion->size;

		iter = OSCollectionIterator::withCollection(currentRegion->dict.get());
		if (iter == nullptr) {
			ok = false;
		}

		while (ok) {
			tmpSymbol = OSDynamicCast(OSSymbol, iter->getNextObject());
			if (tmpSymbol == nullptr) {
				break;
			}

			DEBUG_INFO("adding variable %s\n", tmpSymbol->getCStringNoCopy());

			tmpObject = currentRegion->dict->getObject(tmpSymbol);

			length = maxLength;
			ok = convertObjectToProp(tmpBuffer, &length, tmpSymbol, tmpObject);
			if (ok) {
				tmpBuffer += length;
				maxLength -= length;
			}
		}

		if (!ok) {
			ret = kIOReturnNoSpace;
			IODeleteData(nvramImage, uint8_t, _nvramSize);
			break;
		}

		if (currentRegion->type == kIONVRAMPartitionSystem) {
			_systemUsed = (uint32_t)(tmpBuffer - buffer);
		} else if (currentRegion->type == kIONVRAMPartitionCommon) {
			_commonUsed = (uint32_t)(tmpBuffer - buffer);
		}
	}

	DEBUG_INFO("ok=%d\n", ok);
	require(ok, exit);

	nvram_set_apple_header(nvramImage, _nvramSize, _generation++, _version);

	ret = _nvramController->write(0, nvramImage, _nvramSize);
	DEBUG_INFO("write=%#x\n", ret);

	_nvramController->sync();

	if (_nvramImage) {
		IODeleteData(_nvramImage, uint8_t, _nvramSize);
	}

	_nvramImage = nvramImage;

	_newData = false;

exit:
	return ret;
}

IOReturn
IONVRAMCHRPHandler::setVariable(const uuid_t *varGuid, const char *variableName, OSObject *object)
{
	bool       systemVar = false;
	uint32_t   newSize = 0;
	uint32_t   existingSize = 0;
	bool       remove = (object == nullptr);
	OSObject   *existing;

	if (_systemPartitionSize) {
		if ((uuid_compare(*varGuid, gAppleSystemVariableGuid) == 0) || variableInAllowList(variableName)) {
			systemVar = true;
		}
	}

	DEBUG_INFO("setting %s, systemVar=%d\n", variableName, systemVar);

	if (systemVar) {
		if ((existing = _systemDict->getObject(variableName))) {
			convertObjectToProp(nullptr, &existingSize, variableName, existing);
		}
	} else {
		if ((existing = _commonDict->getObject(variableName))) {
			convertObjectToProp(nullptr, &existingSize, variableName, existing);
		}
	}

	if (remove == false) {
		convertObjectToProp(nullptr, &newSize, variableName, object);

		if (systemVar) {
			if ((newSize + _systemUsed - existingSize) > _systemPartitionSize) {
				DEBUG_ERROR("No space left in system partition, need=%#x, _systemUsed=%#x, _systemPartitionSize=%#x\n",
				    newSize, _systemUsed, _systemPartitionSize);
				return kIOReturnNoSpace;
			} else {
				_systemUsed = _systemUsed + newSize - existingSize;
				_systemDict->setObject(variableName, object);
			}
		} else {
			if ((newSize + _commonUsed - existingSize) > _commonPartitionSize) {
				DEBUG_ERROR("No space left in common partition, need=%#x, _commonUsed=%#x, _commonPartitionSize=%#x\n",
				    newSize, _commonUsed, _commonPartitionSize);
				return kIOReturnNoSpace;
			} else {
				_commonUsed = _commonUsed + newSize - existingSize;
				_commonDict->setObject(variableName, object);
			}
		}

		if (_provider->_diags) {
			_provider->_diags->logVariable(getPartitionTypeForGUID(varGuid), kIONVRAMOperationWrite, variableName, (void *)(uintptr_t)newSize);
		}
	} else {
		if (systemVar) {
			_systemDict->removeObject(variableName);
			_systemUsed -= existingSize;
		} else {
			_commonDict->removeObject(variableName);
			_commonUsed -= existingSize;
		}

		if (_provider->_diags) {
			_provider->_diags->logVariable(getPartitionTypeForGUID(varGuid), kIONVRAMOperationDelete, variableName, nullptr);
		}
	}

	if (_provider->_diags) {
		OSSharedPtr<OSNumber> val = OSNumber::withNumber(getSystemUsed(), 32);
		_provider->_diags->setProperty(kNVRAMSystemUsedKey, val.get());
		DEBUG_INFO("%s=%u\n", kNVRAMSystemUsedKey, getSystemUsed());

		val = OSNumber::withNumber(getCommonUsed(), 32);
		_provider->_diags->setProperty(kNVRAMCommonUsedKey, val.get());
		DEBUG_INFO("%s=%u\n", kNVRAMCommonUsedKey, getCommonUsed());
	}

	_newData = true;

	return kIOReturnSuccess;
}

bool
IONVRAMCHRPHandler::setController(IONVRAMController *controller)
{
	if (_nvramController == NULL) {
		_nvramController = controller;
	}

	return true;
}

bool
IONVRAMCHRPHandler::sync(void)
{
	return serializeVariables() == kIOReturnSuccess;
}

uint32_t
IONVRAMCHRPHandler::getGeneration(void) const
{
	return _generation;
}

uint32_t
IONVRAMCHRPHandler::getVersion(void) const
{
	return _version;
}

uint32_t
IONVRAMCHRPHandler::getSystemUsed(void) const
{
	return _systemUsed;
}

uint32_t
IONVRAMCHRPHandler::getCommonUsed(void) const
{
	return _commonUsed;
}

OSSharedPtr<OSData>
IONVRAMCHRPHandler::unescapeBytesToData(const uint8_t *bytes, uint32_t length)
{
	OSSharedPtr<OSData> data;
	uint32_t            totalLength = 0;
	uint32_t            offset, offset2;
	uint8_t             byte;
	bool                ok;

	// Calculate the actual length of the data.
	ok = true;
	totalLength = 0;
	for (offset = 0; offset < length;) {
		byte = bytes[offset++];
		if (byte == 0xFF) {
			byte = bytes[offset++];
			if (byte == 0x00) {
				ok = false;
				break;
			}
			offset2 = byte & 0x7F;
		} else {
			offset2 = 1;
		}
		totalLength += offset2;
	}

	if (ok) {
		// Create an empty OSData of the correct size.
		data = OSData::withCapacity(totalLength);
		if (data != nullptr) {
			for (offset = 0; offset < length;) {
				byte = bytes[offset++];
				if (byte == 0xFF) {
					byte = bytes[offset++];
					offset2 = byte & 0x7F;
					byte = (byte & 0x80) ? 0xFF : 0x00;
				} else {
					offset2 = 1;
				}
				data->appendByte(byte, offset2);
			}
		}
	}

	return data;
}

OSSharedPtr<OSData>
IONVRAMCHRPHandler::escapeDataToData(OSData * value)
{
	OSSharedPtr<OSData> result;
	const uint8_t       *startPtr;
	const uint8_t       *endPtr;
	const uint8_t       *wherePtr;
	uint8_t             byte;
	bool                ok = true;

	wherePtr = (const uint8_t *) value->getBytesNoCopy();
	endPtr = wherePtr + value->getLength();

	result = OSData::withCapacity((unsigned int)(endPtr - wherePtr));
	if (!result) {
		return result;
	}

	while (wherePtr < endPtr) {
		startPtr = wherePtr;
		byte = *wherePtr++;
		if ((byte == 0x00) || (byte == 0xFF)) {
			for (;
			    ((wherePtr - startPtr) < 0x7F) && (wherePtr < endPtr) && (byte == *wherePtr);
			    wherePtr++) {
			}
			ok &= result->appendByte(0xff, 1);
			byte = (byte & 0x80) | ((uint8_t)(wherePtr - startPtr));
		}
		ok &= result->appendByte(byte, 1);
	}
	ok &= result->appendByte(0, 1);

	if (!ok) {
		result.reset();
	}

	return result;
}

bool
IONVRAMCHRPHandler::convertPropToObject(const uint8_t *propName, uint32_t propNameLength,
    const uint8_t *propData, uint32_t propDataLength,
    const OSSymbol **propSymbol,
    OSObject **propObject)
{
	OSSharedPtr<const OSString> delimitedName;
	OSSharedPtr<const OSSymbol> tmpSymbol;
	OSSharedPtr<OSNumber>       tmpNumber;
	OSSharedPtr<OSString>       tmpString;
	OSSharedPtr<OSObject>       tmpObject = nullptr;

	delimitedName = OSString::withCString((const char *)propName, propNameLength);
	tmpSymbol = OSSymbol::withString(delimitedName.get());

	if (tmpSymbol == nullptr) {
		return false;
	}

	switch (getVariableType(tmpSymbol.get())) {
	case kOFVariableTypeBoolean:
		if (!strncmp("true", (const char *)propData, propDataLength)) {
			tmpObject.reset(kOSBooleanTrue, OSRetain);
		} else if (!strncmp("false", (const char *)propData, propDataLength)) {
			tmpObject.reset(kOSBooleanFalse, OSRetain);
		}
		break;

	case kOFVariableTypeNumber:
		tmpNumber = OSNumber::withNumber(strtol((const char *)propData, nullptr, 0), 32);
		if (tmpNumber != nullptr) {
			tmpObject = tmpNumber;
		}
		break;

	case kOFVariableTypeString:
		tmpString = OSString::withCString((const char *)propData, propDataLength);
		if (tmpString != nullptr) {
			tmpObject = tmpString;
		}
		break;

	case kOFVariableTypeData:
		tmpObject = unescapeBytesToData(propData, propDataLength);
		break;

	default:
		break;
	}

	if (tmpObject == nullptr) {
		tmpSymbol.reset();
		return false;
	}

	*propSymbol = tmpSymbol.detach();
	*propObject = tmpObject.detach();

	return true;
}

bool
IONVRAMCHRPHandler::convertPropToObject(const uint8_t *propName, uint32_t propNameLength,
    const uint8_t *propData, uint32_t propDataLength,
    OSSharedPtr<const OSSymbol>& propSymbol,
    OSSharedPtr<OSObject>& propObject)
{
	const OSSymbol* propSymbolRaw = nullptr;
	OSObject* propObjectRaw       = nullptr;

	bool ok = convertPropToObject(propName, propNameLength, propData, propDataLength,
	    &propSymbolRaw, &propObjectRaw);

	propSymbol.reset(propSymbolRaw, OSNoRetain);
	propObject.reset(propObjectRaw, OSNoRetain);

	return ok;
}

bool
IONVRAMCHRPHandler::convertObjectToProp(uint8_t *buffer, uint32_t *length,
    const OSSymbol *propSymbol, OSObject *propObject)
{
	return convertObjectToProp(buffer, length, propSymbol->getCStringNoCopy(), propObject);
}

bool
IONVRAMCHRPHandler::convertObjectToProp(uint8_t *buffer, uint32_t *length,
    const char *propName, OSObject *propObject)
{
	uint32_t             propNameLength, propDataLength, remaining, offset;
	IONVRAMVariableType  propType;
	OSBoolean            *tmpBoolean = nullptr;
	OSNumber             *tmpNumber = nullptr;
	OSString             *tmpString = nullptr;
	OSSharedPtr<OSData>  tmpData;

	propNameLength = (uint32_t)strlen(propName);
	propType = getVariableType(propName);
	offset = 0;
	remaining = 0;

	// Get the size of the data.
	propDataLength = 0xFFFFFFFF;
	switch (propType) {
	case kOFVariableTypeBoolean:
		tmpBoolean = OSDynamicCast(OSBoolean, propObject);
		if (tmpBoolean != nullptr) {
			propDataLength = 5;
		}
		break;

	case kOFVariableTypeNumber:
		tmpNumber = OSDynamicCast(OSNumber, propObject);
		if (tmpNumber != nullptr) {
			propDataLength = 10;
		}
		break;

	case kOFVariableTypeString:
		tmpString = OSDynamicCast(OSString, propObject);
		if (tmpString != nullptr) {
			propDataLength = tmpString->getLength();
		}
		break;

	case kOFVariableTypeData:
		tmpData.reset(OSDynamicCast(OSData, propObject), OSNoRetain);
		if (tmpData != nullptr) {
			tmpData = escapeDataToData(tmpData.detach());
			// escapeDataToData() adds the NULL byte to the data
			// subtract 1 here to keep offset consistent with the other cases
			propDataLength = tmpData->getLength() - 1;
		}
		break;

	default:
		break;
	}

	// Make sure the propertySize is known and will fit.
	if (propDataLength == 0xFFFFFFFF) {
		return false;
	}

	if (buffer) {
		// name + '=' + data + '\0'
		if ((propNameLength + propDataLength + 2) > *length) {
			return false;
		}

		remaining = *length;
	}

	*length = 0;

	// Copy the property name equal sign.
	offset += snprintf((char *)buffer, remaining, "%s=", propName);
	if (buffer) {
		if (remaining > offset) {
			buffer += offset;
			remaining = remaining - offset;
		} else {
			return false;
		}
	}

	switch (propType) {
	case kOFVariableTypeBoolean:
		if (tmpBoolean->getValue()) {
			offset += strlcpy((char *)buffer, "true", remaining);
		} else {
			offset += strlcpy((char *)buffer, "false", remaining);
		}
		break;

	case kOFVariableTypeNumber:
	{
		uint32_t tmpValue = tmpNumber->unsigned32BitValue();
		if (tmpValue == 0xFFFFFFFF) {
			offset += strlcpy((char *)buffer, "-1", remaining);
		} else if (tmpValue < 1000) {
			offset += snprintf((char *)buffer, remaining, "%d", (uint32_t)tmpValue);
		} else {
			offset += snprintf((char *)buffer, remaining, "%#x", (uint32_t)tmpValue);
		}
	}
	break;

	case kOFVariableTypeString:
		offset += strlcpy((char *)buffer, tmpString->getCStringNoCopy(), remaining);
		break;

	case kOFVariableTypeData:
		if (buffer) {
			bcopy(tmpData->getBytesNoCopy(), buffer, propDataLength);
		}
		tmpData.reset();
		offset += propDataLength;
		break;

	default:
		break;
	}

	*length = offset + 1;

	return true;
}
