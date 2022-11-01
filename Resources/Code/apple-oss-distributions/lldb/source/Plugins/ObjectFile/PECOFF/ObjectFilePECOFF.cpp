//===-- ObjectFilePECOFF.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ObjectFilePECOFF.h"

#include "llvm/Support/COFF.h"

#include "lldb/Core/ArchSpec.h"
#include "lldb/Core/DataBuffer.h"
#include "lldb/Host/FileSpec.h"
#include "lldb/Core/FileSpecList.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Section.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/Timer.h"
#include "lldb/Core/UUID.h"
#include "lldb/Symbol/ObjectFile.h"

#define IMAGE_DOS_SIGNATURE             0x5A4D      // MZ
#define IMAGE_NT_SIGNATURE              0x00004550  // PE00
#define OPT_HEADER_MAGIC_PE32           0x010b
#define OPT_HEADER_MAGIC_PE32_PLUS      0x020b

using namespace lldb;
using namespace lldb_private;

void
ObjectFilePECOFF::Initialize()
{
    PluginManager::RegisterPlugin (GetPluginNameStatic(),
                                   GetPluginDescriptionStatic(),
                                   CreateInstance,
                                   CreateMemoryInstance,
                                   GetModuleSpecifications);
}

void
ObjectFilePECOFF::Terminate()
{
    PluginManager::UnregisterPlugin (CreateInstance);
}


lldb_private::ConstString
ObjectFilePECOFF::GetPluginNameStatic()
{
    static ConstString g_name("pe-coff");
    return g_name;
}

const char *
ObjectFilePECOFF::GetPluginDescriptionStatic()
{
    return "Portable Executable and Common Object File Format object file reader (32 and 64 bit)";
}


ObjectFile *
ObjectFilePECOFF::CreateInstance (const lldb::ModuleSP &module_sp,
                                  DataBufferSP& data_sp,
                                  lldb::offset_t data_offset,
                                  const lldb_private::FileSpec* file,
                                  lldb::offset_t file_offset,
                                  lldb::offset_t length)
{
    if (!data_sp)
    {
        data_sp = file->MemoryMapFileContents(file_offset, length);
        data_offset = 0;
    }

    if (ObjectFilePECOFF::MagicBytesMatch(data_sp))
    {
        // Update the data to contain the entire file if it doesn't already
        if (data_sp->GetByteSize() < length)
            data_sp = file->MemoryMapFileContents(file_offset, length);
        std::unique_ptr<ObjectFile> objfile_ap(new ObjectFilePECOFF (module_sp, data_sp, data_offset, file, file_offset, length));
        if (objfile_ap.get() && objfile_ap->ParseHeader())
            return objfile_ap.release();
    }
    return NULL;
}

ObjectFile *
ObjectFilePECOFF::CreateMemoryInstance (const lldb::ModuleSP &module_sp, 
                                        lldb::DataBufferSP& data_sp, 
                                        const lldb::ProcessSP &process_sp, 
                                        lldb::addr_t header_addr)
{
    return NULL;
}

size_t
ObjectFilePECOFF::GetModuleSpecifications (const lldb_private::FileSpec& file,
                                           lldb::DataBufferSP& data_sp,
                                           lldb::offset_t data_offset,
                                           lldb::offset_t file_offset,
                                           lldb::offset_t length,
                                           lldb_private::ModuleSpecList &specs)
{
    return 0;
}


bool
ObjectFilePECOFF::MagicBytesMatch (DataBufferSP& data_sp)
{
    DataExtractor data(data_sp, eByteOrderLittle, 4);
    lldb::offset_t offset = 0;
    uint16_t magic = data.GetU16 (&offset);
    return magic == IMAGE_DOS_SIGNATURE;
}


ObjectFilePECOFF::ObjectFilePECOFF (const lldb::ModuleSP &module_sp, 
                                    DataBufferSP& data_sp,
                                    lldb::offset_t data_offset,
                                    const FileSpec* file, 
                                    lldb::offset_t file_offset,
                                    lldb::offset_t length) :
    ObjectFile (module_sp, file, file_offset, length, data_sp, data_offset),
    m_dos_header (),
    m_coff_header (),
    m_coff_header_opt (),
    m_sect_headers ()
{
    ::memset (&m_dos_header, 0, sizeof(m_dos_header));
    ::memset (&m_coff_header, 0, sizeof(m_coff_header));
    ::memset (&m_coff_header_opt, 0, sizeof(m_coff_header_opt));
}


ObjectFilePECOFF::~ObjectFilePECOFF()
{
}


bool
ObjectFilePECOFF::ParseHeader ()
{
    ModuleSP module_sp(GetModule());
    if (module_sp)
    {
        lldb_private::Mutex::Locker locker(module_sp->GetMutex());
        m_sect_headers.clear();
        m_data.SetByteOrder (eByteOrderLittle);
        lldb::offset_t offset = 0;
        
        if (ParseDOSHeader())
        {
            offset = m_dos_header.e_lfanew;
            uint32_t pe_signature = m_data.GetU32 (&offset);
            if (pe_signature != IMAGE_NT_SIGNATURE)
                return false;
            if (ParseCOFFHeader(&offset))
            {
                if (m_coff_header.hdrsize > 0)
                    ParseCOFFOptionalHeader(&offset);
                ParseSectionHeaders (offset);
            }
            return true;
        }
    }
    return false;
}


ByteOrder
ObjectFilePECOFF::GetByteOrder () const
{
    return eByteOrderLittle;
}

bool
ObjectFilePECOFF::IsExecutable() const
{
    return (m_coff_header.flags & llvm::COFF::IMAGE_FILE_DLL) == 0;
}

uint32_t
ObjectFilePECOFF::GetAddressByteSize () const
{
    if (m_coff_header_opt.magic == OPT_HEADER_MAGIC_PE32_PLUS)
        return 8;
    else if (m_coff_header_opt.magic == OPT_HEADER_MAGIC_PE32)
        return 4;
    return 4;
}

//----------------------------------------------------------------------
// NeedsEndianSwap
//
// Return true if an endian swap needs to occur when extracting data 
// from this file.
//----------------------------------------------------------------------
bool 
ObjectFilePECOFF::NeedsEndianSwap() const
{
#if defined(__LITTLE_ENDIAN__)
    return false;
#else
    return true;
#endif
}
//----------------------------------------------------------------------
// ParseDOSHeader
//----------------------------------------------------------------------
bool
ObjectFilePECOFF::ParseDOSHeader ()
{
    bool success = false;
    lldb::offset_t offset = 0;
    success = m_data.ValidOffsetForDataOfSize(0, sizeof(m_dos_header));
    
    if (success)
    {
        m_dos_header.e_magic = m_data.GetU16(&offset); // Magic number
        success = m_dos_header.e_magic == IMAGE_DOS_SIGNATURE;
        
        if (success)
        {
            m_dos_header.e_cblp     = m_data.GetU16(&offset); // Bytes on last page of file
            m_dos_header.e_cp       = m_data.GetU16(&offset); // Pages in file
            m_dos_header.e_crlc     = m_data.GetU16(&offset); // Relocations
            m_dos_header.e_cparhdr  = m_data.GetU16(&offset); // Size of header in paragraphs
            m_dos_header.e_minalloc = m_data.GetU16(&offset); // Minimum extra paragraphs needed
            m_dos_header.e_maxalloc = m_data.GetU16(&offset); // Maximum extra paragraphs needed
            m_dos_header.e_ss       = m_data.GetU16(&offset); // Initial (relative) SS value
            m_dos_header.e_sp       = m_data.GetU16(&offset); // Initial SP value
            m_dos_header.e_csum     = m_data.GetU16(&offset); // Checksum
            m_dos_header.e_ip       = m_data.GetU16(&offset); // Initial IP value
            m_dos_header.e_cs       = m_data.GetU16(&offset); // Initial (relative) CS value
            m_dos_header.e_lfarlc   = m_data.GetU16(&offset); // File address of relocation table
            m_dos_header.e_ovno     = m_data.GetU16(&offset); // Overlay number
            
            m_dos_header.e_res[0]   = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res[1]   = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res[2]   = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res[3]   = m_data.GetU16(&offset); // Reserved words
            
            m_dos_header.e_oemid    = m_data.GetU16(&offset); // OEM identifier (for e_oeminfo)
            m_dos_header.e_oeminfo  = m_data.GetU16(&offset); // OEM information; e_oemid specific
            m_dos_header.e_res2[0]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[1]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[2]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[3]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[4]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[5]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[6]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[7]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[8]  = m_data.GetU16(&offset); // Reserved words
            m_dos_header.e_res2[9]  = m_data.GetU16(&offset); // Reserved words
            
            m_dos_header.e_lfanew   = m_data.GetU32(&offset); // File address of new exe header
        }
    }
    if (!success)
        memset(&m_dos_header, 0, sizeof(m_dos_header));
    return success;
}


//----------------------------------------------------------------------
// ParserCOFFHeader
//----------------------------------------------------------------------
bool
ObjectFilePECOFF::ParseCOFFHeader(lldb::offset_t *offset_ptr)
{
    bool success = m_data.ValidOffsetForDataOfSize (*offset_ptr, sizeof(m_coff_header));
    if (success)
    {
        m_coff_header.machine   = m_data.GetU16(offset_ptr);
        m_coff_header.nsects    = m_data.GetU16(offset_ptr);
        m_coff_header.modtime   = m_data.GetU32(offset_ptr);
        m_coff_header.symoff    = m_data.GetU32(offset_ptr);
        m_coff_header.nsyms     = m_data.GetU32(offset_ptr);
        m_coff_header.hdrsize   = m_data.GetU16(offset_ptr);
        m_coff_header.flags     = m_data.GetU16(offset_ptr);
    }
    if (!success)
        memset(&m_coff_header, 0, sizeof(m_coff_header));
    return success;
}

bool
ObjectFilePECOFF::ParseCOFFOptionalHeader(lldb::offset_t *offset_ptr)
{
    bool success = false;
    const lldb::offset_t end_offset = *offset_ptr + m_coff_header.hdrsize;
    if (*offset_ptr < end_offset)
    {
        success = true;
        m_coff_header_opt.magic                         = m_data.GetU16(offset_ptr); 
        m_coff_header_opt.major_linker_version          = m_data.GetU8 (offset_ptr);
        m_coff_header_opt.minor_linker_version          = m_data.GetU8 (offset_ptr);     
        m_coff_header_opt.code_size                     = m_data.GetU32(offset_ptr); 
        m_coff_header_opt.data_size                     = m_data.GetU32(offset_ptr); 
        m_coff_header_opt.bss_size                      = m_data.GetU32(offset_ptr); 
        m_coff_header_opt.entry                         = m_data.GetU32(offset_ptr); 
        m_coff_header_opt.code_offset                   = m_data.GetU32(offset_ptr); 

        const uint32_t addr_byte_size = GetAddressByteSize ();

        if (*offset_ptr < end_offset)
        {
            if (m_coff_header_opt.magic == OPT_HEADER_MAGIC_PE32)
            {
                // PE32 only
                m_coff_header_opt.data_offset               = m_data.GetU32(offset_ptr);                             
            }
            else
                m_coff_header_opt.data_offset = 0;
        
            if (*offset_ptr < end_offset)
            {
                m_coff_header_opt.image_base                    = m_data.GetMaxU64 (offset_ptr, addr_byte_size); 
                m_coff_header_opt.sect_alignment                = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.file_alignment                = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.major_os_system_version       = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.minor_os_system_version       = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.major_image_version           = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.minor_image_version           = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.major_subsystem_version       = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.minor_subsystem_version       = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.reserved1                     = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.image_size                    = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.header_size                   = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.checksum                      = m_data.GetU32(offset_ptr); 
                m_coff_header_opt.subsystem                     = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.dll_flags                     = m_data.GetU16(offset_ptr); 
                m_coff_header_opt.stack_reserve_size            = m_data.GetMaxU64 (offset_ptr, addr_byte_size);
                m_coff_header_opt.stack_commit_size             = m_data.GetMaxU64 (offset_ptr, addr_byte_size);
                m_coff_header_opt.heap_reserve_size             = m_data.GetMaxU64 (offset_ptr, addr_byte_size);
                m_coff_header_opt.heap_commit_size              = m_data.GetMaxU64 (offset_ptr, addr_byte_size);
                m_coff_header_opt.loader_flags                  = m_data.GetU32(offset_ptr); 
                uint32_t num_data_dir_entries = m_data.GetU32(offset_ptr);
                m_coff_header_opt.data_dirs.clear();
                m_coff_header_opt.data_dirs.resize(num_data_dir_entries);
                uint32_t i;
                for (i=0; i<num_data_dir_entries; i++)
                {
                    m_coff_header_opt.data_dirs[i].vmaddr = m_data.GetU32(offset_ptr);
                    m_coff_header_opt.data_dirs[i].vmsize = m_data.GetU32(offset_ptr);
                }
            }
        }
    }
    // Make sure we are on track for section data which follows
    *offset_ptr = end_offset;
    return success;
}


//----------------------------------------------------------------------
// ParseSectionHeaders
//----------------------------------------------------------------------
bool
ObjectFilePECOFF::ParseSectionHeaders (uint32_t section_header_data_offset)
{
    const uint32_t nsects = m_coff_header.nsects;
    m_sect_headers.clear();
    
    if (nsects > 0)
    {
        const uint32_t addr_byte_size = GetAddressByteSize ();
        const size_t section_header_byte_size = nsects * sizeof(section_header_t);
        DataBufferSP section_header_data_sp(m_file.ReadFileContents (section_header_data_offset, section_header_byte_size));
        DataExtractor section_header_data (section_header_data_sp, GetByteOrder(), addr_byte_size);

        lldb::offset_t offset = 0;
        if (section_header_data.ValidOffsetForDataOfSize (offset, section_header_byte_size))
        {
            m_sect_headers.resize(nsects);
            
            for (uint32_t idx = 0; idx<nsects; ++idx)
            {
                const void *name_data = section_header_data.GetData(&offset, 8);
                if (name_data)
                {
                    memcpy(m_sect_headers[idx].name, name_data, 8);
                    m_sect_headers[idx].vmsize  = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].vmaddr  = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].size    = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].offset  = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].reloff  = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].lineoff = section_header_data.GetU32(&offset);
                    m_sect_headers[idx].nreloc  = section_header_data.GetU16(&offset);
                    m_sect_headers[idx].nline   = section_header_data.GetU16(&offset);
                    m_sect_headers[idx].flags   = section_header_data.GetU32(&offset);
                }
            }
        }
    }
    
    return m_sect_headers.empty() == false;
}

bool
ObjectFilePECOFF::GetSectionName(std::string& sect_name, const section_header_t& sect)
{
    if (sect.name[0] == '/')
    {
        lldb::offset_t stroff = strtoul(&sect.name[1], NULL, 10);
        lldb::offset_t string_file_offset = m_coff_header.symoff + (m_coff_header.nsyms * 18) + stroff;
        const char *name = m_data.GetCStr (&string_file_offset);
        if (name)
        {
            sect_name = name;
            return true;
        }
        
        return false;
    }
    sect_name = sect.name;
    return true;
}       

//----------------------------------------------------------------------
// GetNListSymtab
//----------------------------------------------------------------------
Symtab *
ObjectFilePECOFF::GetSymtab()
{
    ModuleSP module_sp(GetModule());
    if (module_sp)
    {
        lldb_private::Mutex::Locker locker(module_sp->GetMutex());
        if (m_symtab_ap.get() == NULL)
        {
            SectionList *sect_list = GetSectionList();
            m_symtab_ap.reset(new Symtab(this));
            Mutex::Locker symtab_locker (m_symtab_ap->GetMutex());
            
            const uint32_t num_syms = m_coff_header.nsyms;

            if (num_syms > 0 && m_coff_header.symoff > 0)
            {
                const uint32_t symbol_size = 18;
                const uint32_t addr_byte_size = GetAddressByteSize ();
                const size_t symbol_data_size = num_syms * symbol_size; 
                // Include the 4 bytes string table size at the end of the symbols
                DataBufferSP symtab_data_sp(m_file.ReadFileContents (m_coff_header.symoff, symbol_data_size + 4));
                DataExtractor symtab_data (symtab_data_sp, GetByteOrder(), addr_byte_size);
                lldb::offset_t offset = symbol_data_size;
                const uint32_t strtab_size = symtab_data.GetU32 (&offset);
                DataBufferSP strtab_data_sp(m_file.ReadFileContents (m_coff_header.symoff + symbol_data_size, strtab_size));
                DataExtractor strtab_data (strtab_data_sp, GetByteOrder(), addr_byte_size);

                // First 4 bytes should be zeroed after strtab_size has been read,
                // because it is used as offset 0 to encode a NULL string.
                uint32_t* strtab_data_start = (uint32_t*)strtab_data_sp->GetBytes();
                strtab_data_start[0] = 0;

                offset = 0;
                std::string symbol_name;
                Symbol *symbols = m_symtab_ap->Resize (num_syms);
                for (uint32_t i=0; i<num_syms; ++i)
                {
                    coff_symbol_t symbol;
                    const uint32_t symbol_offset = offset;
                    const char *symbol_name_cstr = NULL;
                    // If the first 4 bytes of the symbol string are zero, then we
                    // it is followed by a 4 byte string table offset. Else these
                    // 8 bytes contain the symbol name
                    if (symtab_data.GetU32 (&offset) == 0)
                    {
                        // Long string that doesn't fit into the symbol table name,
                        // so now we must read the 4 byte string table offset
                        uint32_t strtab_offset = symtab_data.GetU32 (&offset);
                        symbol_name_cstr = strtab_data.PeekCStr (strtab_offset);
                        symbol_name.assign (symbol_name_cstr);
                    }
                    else
                    {
                        // Short string that fits into the symbol table name which is 8 bytes
                        offset += sizeof(symbol.name) - 4; // Skip remaining 
                        symbol_name_cstr = symtab_data.PeekCStr (symbol_offset);
                        if (symbol_name_cstr == NULL)
                            break;
                        symbol_name.assign (symbol_name_cstr, sizeof(symbol.name));
                    }
                    symbol.value    = symtab_data.GetU32 (&offset);
                    symbol.sect     = symtab_data.GetU16 (&offset);
                    symbol.type     = symtab_data.GetU16 (&offset);
                    symbol.storage  = symtab_data.GetU8  (&offset);
                    symbol.naux     = symtab_data.GetU8  (&offset);		
                    symbols[i].GetMangled ().SetValue (ConstString(symbol_name.c_str()));
                    if ((int16_t)symbol.sect >= 1)
                    {
                        Address symbol_addr(sect_list->GetSectionAtIndex(symbol.sect-1), symbol.value);
                        symbols[i].GetAddress() = symbol_addr;
                    }

                    if (symbol.naux > 0)
                    {
                        i += symbol.naux;
                        offset += symbol_size;
                    }
                }
                
            }

            // Read export header
            if (coff_data_dir_export_table < m_coff_header_opt.data_dirs.size()
                && m_coff_header_opt.data_dirs[coff_data_dir_export_table].vmsize > 0 && m_coff_header_opt.data_dirs[coff_data_dir_export_table].vmaddr > 0)
            {
                export_directory_entry export_table;
                uint32_t data_start = m_coff_header_opt.data_dirs[coff_data_dir_export_table].vmaddr;
                Address address(m_coff_header_opt.image_base + data_start, sect_list);
                DataBufferSP symtab_data_sp(m_file.ReadFileContents(address.GetSection()->GetFileOffset() + address.GetOffset(), m_coff_header_opt.data_dirs[0].vmsize));
                DataExtractor symtab_data (symtab_data_sp, GetByteOrder(), GetAddressByteSize());
                lldb::offset_t offset = 0;

                // Read export_table header
                export_table.characteristics = symtab_data.GetU32(&offset);
                export_table.time_date_stamp = symtab_data.GetU32(&offset);
                export_table.major_version = symtab_data.GetU16(&offset);
                export_table.minor_version = symtab_data.GetU16(&offset);
                export_table.name = symtab_data.GetU32(&offset);
                export_table.base = symtab_data.GetU32(&offset);
                export_table.number_of_functions = symtab_data.GetU32(&offset);
                export_table.number_of_names = symtab_data.GetU32(&offset);
                export_table.address_of_functions = symtab_data.GetU32(&offset);
                export_table.address_of_names = symtab_data.GetU32(&offset);
                export_table.address_of_name_ordinals = symtab_data.GetU32(&offset);

                bool has_ordinal = export_table.address_of_name_ordinals != 0;

                lldb::offset_t name_offset = export_table.address_of_names - data_start;
                lldb::offset_t name_ordinal_offset = export_table.address_of_name_ordinals - data_start;

                Symbol *symbols = m_symtab_ap->Resize(export_table.number_of_names);

                std::string symbol_name;

                // Read each export table entry
                for (size_t i = 0; i < export_table.number_of_names; ++i)
                {
                    uint32_t name_ordinal = has_ordinal ? symtab_data.GetU16(&name_ordinal_offset) : i;
                    uint32_t name_address = symtab_data.GetU32(&name_offset);

                    const char* symbol_name_cstr = symtab_data.PeekCStr(name_address - data_start);
                    symbol_name.assign(symbol_name_cstr);

                    lldb::offset_t function_offset = export_table.address_of_functions - data_start + sizeof(uint32_t) * name_ordinal;
                    uint32_t function_rva = symtab_data.GetU32(&function_offset);

                    Address symbol_addr(m_coff_header_opt.image_base + function_rva, sect_list);
                    symbols[i].GetMangled().SetValue(ConstString(symbol_name.c_str()));
                    symbols[i].GetAddress() = symbol_addr;
                    symbols[i].SetType(lldb::eSymbolTypeCode);
                    symbols[i].SetDebug(true);
                }
            }
        }
    }
    return m_symtab_ap.get();

}

bool
ObjectFilePECOFF::IsStripped ()
{
    // TODO: determine this for COFF
    return false;
}



void
ObjectFilePECOFF::CreateSections (SectionList &unified_section_list)
{
    if (!m_sections_ap.get())
    {
        m_sections_ap.reset(new SectionList());

        ModuleSP module_sp(GetModule());
        if (module_sp)
        {
            lldb_private::Mutex::Locker locker(module_sp->GetMutex());
            const uint32_t nsects = m_sect_headers.size();
            ModuleSP module_sp (GetModule());
            for (uint32_t idx = 0; idx<nsects; ++idx)
            {
                std::string sect_name;
                GetSectionName (sect_name, m_sect_headers[idx]);
                ConstString const_sect_name (sect_name.c_str());
                static ConstString g_code_sect_name (".code");
                static ConstString g_CODE_sect_name ("CODE");
                static ConstString g_data_sect_name (".data");
                static ConstString g_DATA_sect_name ("DATA");
                static ConstString g_bss_sect_name (".bss");
                static ConstString g_BSS_sect_name ("BSS");
                static ConstString g_debug_sect_name (".debug");
                static ConstString g_reloc_sect_name (".reloc");
                static ConstString g_stab_sect_name (".stab");
                static ConstString g_stabstr_sect_name (".stabstr");
                static ConstString g_sect_name_dwarf_debug_abbrev (".debug_abbrev");
                static ConstString g_sect_name_dwarf_debug_aranges (".debug_aranges");
                static ConstString g_sect_name_dwarf_debug_frame (".debug_frame");
                static ConstString g_sect_name_dwarf_debug_info (".debug_info");
                static ConstString g_sect_name_dwarf_debug_line (".debug_line");
                static ConstString g_sect_name_dwarf_debug_loc (".debug_loc");
                static ConstString g_sect_name_dwarf_debug_macinfo (".debug_macinfo");
                static ConstString g_sect_name_dwarf_debug_pubnames (".debug_pubnames");
                static ConstString g_sect_name_dwarf_debug_pubtypes (".debug_pubtypes");
                static ConstString g_sect_name_dwarf_debug_ranges (".debug_ranges");
                static ConstString g_sect_name_dwarf_debug_str (".debug_str");
                static ConstString g_sect_name_eh_frame (".eh_frame");
                SectionType section_type = eSectionTypeOther;
                if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_CODE &&
                    ((const_sect_name == g_code_sect_name) || (const_sect_name == g_CODE_sect_name)))
                {
                    section_type = eSectionTypeCode;
                }
                else if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_INITIALIZED_DATA &&
                         ((const_sect_name == g_data_sect_name) || (const_sect_name == g_DATA_sect_name)))
                {
                    section_type = eSectionTypeData;
                }
                else if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_UNINITIALIZED_DATA &&
                         ((const_sect_name == g_bss_sect_name) || (const_sect_name == g_BSS_sect_name)))
                {
                    if (m_sect_headers[idx].size == 0)
                        section_type = eSectionTypeZeroFill;
                    else
                        section_type = eSectionTypeData;
                }
                else if (const_sect_name == g_debug_sect_name)
                {
                    section_type = eSectionTypeDebug;
                }
                else if (const_sect_name == g_stabstr_sect_name)
                {
                    section_type = eSectionTypeDataCString;
                }
                else if (const_sect_name == g_reloc_sect_name)
                {
                    section_type = eSectionTypeOther;
                }
                else if (const_sect_name == g_sect_name_dwarf_debug_abbrev)    section_type = eSectionTypeDWARFDebugAbbrev;
                else if (const_sect_name == g_sect_name_dwarf_debug_aranges)   section_type = eSectionTypeDWARFDebugAranges;
                else if (const_sect_name == g_sect_name_dwarf_debug_frame)     section_type = eSectionTypeDWARFDebugFrame;
                else if (const_sect_name == g_sect_name_dwarf_debug_info)      section_type = eSectionTypeDWARFDebugInfo;
                else if (const_sect_name == g_sect_name_dwarf_debug_line)      section_type = eSectionTypeDWARFDebugLine;
                else if (const_sect_name == g_sect_name_dwarf_debug_loc)       section_type = eSectionTypeDWARFDebugLoc;
                else if (const_sect_name == g_sect_name_dwarf_debug_macinfo)   section_type = eSectionTypeDWARFDebugMacInfo;
                else if (const_sect_name == g_sect_name_dwarf_debug_pubnames)  section_type = eSectionTypeDWARFDebugPubNames;
                else if (const_sect_name == g_sect_name_dwarf_debug_pubtypes)  section_type = eSectionTypeDWARFDebugPubTypes;
                else if (const_sect_name == g_sect_name_dwarf_debug_ranges)    section_type = eSectionTypeDWARFDebugRanges;
                else if (const_sect_name == g_sect_name_dwarf_debug_str)       section_type = eSectionTypeDWARFDebugStr;
                else if (const_sect_name == g_sect_name_eh_frame)              section_type = eSectionTypeEHFrame;
                else if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_CODE)
                {
                    section_type = eSectionTypeCode;
                }
                else if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_INITIALIZED_DATA)
                {
                    section_type = eSectionTypeData;
                }
                else if (m_sect_headers[idx].flags & llvm::COFF::IMAGE_SCN_CNT_UNINITIALIZED_DATA)
                {
                    if (m_sect_headers[idx].size == 0)
                        section_type = eSectionTypeZeroFill;
                    else
                        section_type = eSectionTypeData;
                }

                // Use a segment ID of the segment index shifted left by 8 so they
                // never conflict with any of the sections.
                SectionSP section_sp (new Section (module_sp,                    // Module to which this section belongs
                                                   this,                         // Object file to which this section belongs
                                                   idx + 1,                      // Section ID is the 1 based segment index shifted right by 8 bits as not to collide with any of the 256 section IDs that are possible
                                                   const_sect_name,              // Name of this section
                                                   section_type,                 // This section is a container of other sections.
                                                   m_coff_header_opt.image_base + m_sect_headers[idx].vmaddr,   // File VM address == addresses as they are found in the object file
                                                   m_sect_headers[idx].vmsize,   // VM size in bytes of this section
                                                   m_sect_headers[idx].offset,   // Offset to the data for this section in the file
                                                   m_sect_headers[idx].size,     // Size in bytes of this section as found in the the file
                                                   m_sect_headers[idx].flags));  // Flags for this section

                //section_sp->SetIsEncrypted (segment_is_encrypted);

                unified_section_list.AddSection(section_sp);
                m_sections_ap->AddSection (section_sp);
            }
        }
    }
}

bool
ObjectFilePECOFF::GetUUID (UUID* uuid)
{
    return false;
}

uint32_t
ObjectFilePECOFF::GetDependentModules (FileSpecList& files)
{
    return 0;
}


//----------------------------------------------------------------------
// Dump
//
// Dump the specifics of the runtime file container (such as any headers
// segments, sections, etc).
//----------------------------------------------------------------------
void 
ObjectFilePECOFF::Dump(Stream *s)
{
    ModuleSP module_sp(GetModule());
    if (module_sp)
    {
        lldb_private::Mutex::Locker locker(module_sp->GetMutex());
        s->Printf("%p: ", this);
        s->Indent();
        s->PutCString("ObjectFilePECOFF");
        
        ArchSpec header_arch;
        GetArchitecture (header_arch);
        
        *s << ", file = '" << m_file << "', arch = " << header_arch.GetArchitectureName() << "\n";
        
        SectionList *sections = GetSectionList();
        if (sections)
            sections->Dump(s, NULL, true, UINT32_MAX);
        
        if (m_symtab_ap.get())
            m_symtab_ap->Dump(s, NULL, eSortOrderNone);

        if (m_dos_header.e_magic)
            DumpDOSHeader (s, m_dos_header);
        if (m_coff_header.machine)
        {
            DumpCOFFHeader (s, m_coff_header);
            if (m_coff_header.hdrsize)
                DumpOptCOFFHeader (s, m_coff_header_opt);
        }
        s->EOL();
        DumpSectionHeaders(s);
        s->EOL();
    }
}

//----------------------------------------------------------------------
// DumpDOSHeader
//
// Dump the MS-DOS header to the specified output stream
//----------------------------------------------------------------------
void
ObjectFilePECOFF::DumpDOSHeader(Stream *s, const dos_header_t& header)
{
    s->PutCString ("MSDOS Header\n");
    s->Printf ("  e_magic    = 0x%4.4x\n", header.e_magic);
    s->Printf ("  e_cblp     = 0x%4.4x\n", header.e_cblp);
    s->Printf ("  e_cp       = 0x%4.4x\n", header.e_cp);
    s->Printf ("  e_crlc     = 0x%4.4x\n", header.e_crlc);
    s->Printf ("  e_cparhdr  = 0x%4.4x\n", header.e_cparhdr);
    s->Printf ("  e_minalloc = 0x%4.4x\n", header.e_minalloc);
    s->Printf ("  e_maxalloc = 0x%4.4x\n", header.e_maxalloc);
    s->Printf ("  e_ss       = 0x%4.4x\n", header.e_ss);
    s->Printf ("  e_sp       = 0x%4.4x\n", header.e_sp);
    s->Printf ("  e_csum     = 0x%4.4x\n", header.e_csum);
    s->Printf ("  e_ip       = 0x%4.4x\n", header.e_ip);
    s->Printf ("  e_cs       = 0x%4.4x\n", header.e_cs);
    s->Printf ("  e_lfarlc   = 0x%4.4x\n", header.e_lfarlc);
    s->Printf ("  e_ovno     = 0x%4.4x\n", header.e_ovno);
    s->Printf ("  e_res[4]   = { 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x }\n",
               header.e_res[0],
               header.e_res[1],
               header.e_res[2],
               header.e_res[3]);
    s->Printf ("  e_oemid    = 0x%4.4x\n", header.e_oemid);
    s->Printf ("  e_oeminfo  = 0x%4.4x\n", header.e_oeminfo);
    s->Printf ("  e_res2[10] = { 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x, 0x%4.4x }\n",
               header.e_res2[0],
               header.e_res2[1],
               header.e_res2[2],
               header.e_res2[3],
               header.e_res2[4],
               header.e_res2[5],
               header.e_res2[6],
               header.e_res2[7],
               header.e_res2[8],
               header.e_res2[9]);
    s->Printf ("  e_lfanew   = 0x%8.8x\n", header.e_lfanew);
}

//----------------------------------------------------------------------
// DumpCOFFHeader
//
// Dump the COFF header to the specified output stream
//----------------------------------------------------------------------
void
ObjectFilePECOFF::DumpCOFFHeader(Stream *s, const coff_header_t& header)
{
    s->PutCString ("COFF Header\n");
    s->Printf ("  machine = 0x%4.4x\n", header.machine);
    s->Printf ("  nsects  = 0x%4.4x\n", header.nsects);
    s->Printf ("  modtime = 0x%8.8x\n", header.modtime);
    s->Printf ("  symoff  = 0x%8.8x\n", header.symoff);
    s->Printf ("  nsyms   = 0x%8.8x\n", header.nsyms);
    s->Printf ("  hdrsize = 0x%4.4x\n", header.hdrsize);
}

//----------------------------------------------------------------------
// DumpOptCOFFHeader
//
// Dump the optional COFF header to the specified output stream
//----------------------------------------------------------------------
void
ObjectFilePECOFF::DumpOptCOFFHeader(Stream *s, const coff_opt_header_t& header)
{
    s->PutCString ("Optional COFF Header\n");
    s->Printf ("  magic                   = 0x%4.4x\n", header.magic);
    s->Printf ("  major_linker_version    = 0x%2.2x\n", header.major_linker_version);
    s->Printf ("  minor_linker_version    = 0x%2.2x\n", header.minor_linker_version);
    s->Printf ("  code_size               = 0x%8.8x\n", header.code_size);
    s->Printf ("  data_size               = 0x%8.8x\n", header.data_size);
    s->Printf ("  bss_size                = 0x%8.8x\n", header.bss_size);
    s->Printf ("  entry                   = 0x%8.8x\n", header.entry);
    s->Printf ("  code_offset             = 0x%8.8x\n", header.code_offset);
    s->Printf ("  data_offset             = 0x%8.8x\n", header.data_offset);
    s->Printf ("  image_base              = 0x%16.16" PRIx64 "\n", header.image_base);
    s->Printf ("  sect_alignment          = 0x%8.8x\n", header.sect_alignment);
    s->Printf ("  file_alignment          = 0x%8.8x\n", header.file_alignment);
    s->Printf ("  major_os_system_version = 0x%4.4x\n", header.major_os_system_version);
    s->Printf ("  minor_os_system_version = 0x%4.4x\n", header.minor_os_system_version);
    s->Printf ("  major_image_version     = 0x%4.4x\n", header.major_image_version);
    s->Printf ("  minor_image_version     = 0x%4.4x\n", header.minor_image_version);
    s->Printf ("  major_subsystem_version = 0x%4.4x\n", header.major_subsystem_version);
    s->Printf ("  minor_subsystem_version = 0x%4.4x\n", header.minor_subsystem_version);
    s->Printf ("  reserved1               = 0x%8.8x\n", header.reserved1);
    s->Printf ("  image_size              = 0x%8.8x\n", header.image_size);
    s->Printf ("  header_size             = 0x%8.8x\n", header.header_size);
    s->Printf ("  checksum                = 0x%8.8x\n", header.checksum);
    s->Printf ("  subsystem               = 0x%4.4x\n", header.subsystem);
    s->Printf ("  dll_flags               = 0x%4.4x\n", header.dll_flags);
    s->Printf ("  stack_reserve_size      = 0x%16.16" PRIx64 "\n", header.stack_reserve_size);
    s->Printf ("  stack_commit_size       = 0x%16.16" PRIx64 "\n", header.stack_commit_size);
    s->Printf ("  heap_reserve_size       = 0x%16.16" PRIx64 "\n", header.heap_reserve_size);
    s->Printf ("  heap_commit_size        = 0x%16.16" PRIx64 "\n", header.heap_commit_size);
    s->Printf ("  loader_flags            = 0x%8.8x\n", header.loader_flags);
    s->Printf ("  num_data_dir_entries    = 0x%8.8zx\n", header.data_dirs.size());
    uint32_t i;
    for (i=0; i<header.data_dirs.size(); i++)
    {
        s->Printf ("  data_dirs[%2u] vmaddr = 0x%8.8x, vmsize = 0x%8.8x\n", 
                   i,
                   header.data_dirs[i].vmaddr,
                   header.data_dirs[i].vmsize);
    }
}
//----------------------------------------------------------------------
// DumpSectionHeader
//
// Dump a single ELF section header to the specified output stream
//----------------------------------------------------------------------
void
ObjectFilePECOFF::DumpSectionHeader(Stream *s, const section_header_t& sh)
{
    std::string name;
    GetSectionName(name, sh);
    s->Printf ("%-16s 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%4.4x 0x%4.4x 0x%8.8x\n",
               name.c_str(),
               sh.vmaddr,
               sh.vmsize,
               sh.offset,
               sh.size,
               sh.reloff,
               sh.lineoff,
               sh.nreloc,
               sh.nline,
               sh.flags);
}


//----------------------------------------------------------------------
// DumpSectionHeaders
//
// Dump all of the ELF section header to the specified output stream
//----------------------------------------------------------------------
void
ObjectFilePECOFF::DumpSectionHeaders(Stream *s)
{
    
    s->PutCString ("Section Headers\n");
    s->PutCString ("IDX  name             vm addr    vm size    file off   file size  reloc off  line off   nreloc nline  flags\n");
    s->PutCString ("==== ---------------- ---------- ---------- ---------- ---------- ---------- ---------- ------ ------ ----------\n");
    
    uint32_t idx = 0;
    SectionHeaderCollIter pos, end = m_sect_headers.end();
    
    for (pos = m_sect_headers.begin(); pos != end; ++pos, ++idx)
    {
        s->Printf ("[%2u] ", idx);
        ObjectFilePECOFF::DumpSectionHeader(s, *pos);
    }
}

bool
ObjectFilePECOFF::GetArchitecture (ArchSpec &arch)
{
    uint16_t machine = m_coff_header.machine;
    switch (machine)
    {
        case llvm::COFF::IMAGE_FILE_MACHINE_AMD64:
        case llvm::COFF::IMAGE_FILE_MACHINE_I386:
        case llvm::COFF::IMAGE_FILE_MACHINE_POWERPC:
        case llvm::COFF::IMAGE_FILE_MACHINE_POWERPCFP:
        case llvm::COFF::IMAGE_FILE_MACHINE_ARM:
        case llvm::COFF::IMAGE_FILE_MACHINE_ARMV7:
        case llvm::COFF::IMAGE_FILE_MACHINE_THUMB:
            arch.SetArchitecture (eArchTypeCOFF, machine, LLDB_INVALID_CPUTYPE);
            return true;
        default:
            break;
    }
    return false;
}

ObjectFile::Type
ObjectFilePECOFF::CalculateType()
{
    if (m_coff_header.machine != 0)
    {
        if ((m_coff_header.flags & llvm::COFF::IMAGE_FILE_DLL) == 0)
            return eTypeExecutable;
        else
            return eTypeSharedLibrary;
    }
    return eTypeExecutable;
}

ObjectFile::Strata
ObjectFilePECOFF::CalculateStrata()
{
    return eStrataUser;
}
//------------------------------------------------------------------
// PluginInterface protocol
//------------------------------------------------------------------
ConstString
ObjectFilePECOFF::GetPluginName()
{
    return GetPluginNameStatic();
}

uint32_t
ObjectFilePECOFF::GetPluginVersion()
{
    return 1;
}

