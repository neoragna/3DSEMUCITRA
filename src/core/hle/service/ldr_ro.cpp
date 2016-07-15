// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/swap.h"

#include "core/arm/arm_interface.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/ldr_ro.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

// GCC versions < 5.0 do not implement std::is_trivially_copyable.
// Excluding MSVC because it has weird behaviour for std::is_trivially_copyable.
#if (__GNUC__ >= 5) || defined(__clang__)
    #define ASSERT_CRO_STRUCT(name, size) \
        static_assert(std::is_standard_layout<name>::value, "CRO structure " #name " doesn't use standard layout"); \
        static_assert(std::is_trivially_copyable<name>::value, "CRO structure " #name " isn't trivially copyable"); \
        static_assert(sizeof(name) == (size), "Unexpected struct size for CRO structure " #name)
#else
    #define ASSERT_CRO_STRUCT(name, size) \
        static_assert(std::is_standard_layout<name>::value, "CRO structure " #name " doesn't use standard layout"); \
        static_assert(sizeof(name) == (size), "Unexpected struct size for CRO structure " #name)
#endif

static constexpr u32 CRO_HEADER_SIZE = 0x138;
static constexpr u32 CRO_HASH_SIZE = 0x80;

static const ResultCode ERROR_ALREADY_INITIALIZED =   // 0xD9612FF9
    ResultCode(ErrorDescription::AlreadyInitialized,         ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_INITIALIZED =       // 0xD9612FF8
    ResultCode(ErrorDescription::NotInitialized,             ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Permanent);
static const ResultCode ERROR_BUFFER_TOO_SMALL =      // 0xE0E12C1F
    ResultCode(static_cast<ErrorDescription>(31),            ErrorModule::RO, ErrorSummary::InvalidArgument, ErrorLevel::Usage);
static const ResultCode ERROR_MISALIGNED_ADDRESS =    // 0xD9012FF1
    ResultCode(ErrorDescription::MisalignedAddress,          ErrorModule::RO, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);
static const ResultCode ERROR_MISALIGNED_SIZE =       // 0xD9012FF2
    ResultCode(ErrorDescription::MisalignedSize,             ErrorModule::RO, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);
static const ResultCode ERROR_ILLEGAL_ADDRESS =       // 0xE1612C0F
    ResultCode(static_cast<ErrorDescription>(15),            ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Usage);
static const ResultCode ERROR_INVALID_MEMORY_STATE =  // 0xD8A12C08
    ResultCode(static_cast<ErrorDescription>(8),             ErrorModule::RO, ErrorSummary::InvalidState,    ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_LOADED =            // 0xD8A12C0D
    ResultCode(static_cast<ErrorDescription>(13),            ErrorModule::RO, ErrorSummary::InvalidState,    ErrorLevel::Permanent);
static const ResultCode ERROR_INVALID_DESCRIPTOR =    // 0xD9001830
    ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);

static ResultCode CROFormatError(u32 description) {
    return ResultCode(static_cast<ErrorDescription>(description), ErrorModule::RO, ErrorSummary::WrongArgument, ErrorLevel::Permanent);
}

/// Represents a loaded module (CRO) with interfaces manipulating it.
class CROHelper final {
    const VAddr address; ///< the virtual address of this module

    /**
     * Each item in this enum represents a u32 field in the header begin from address+0x80, successively.
     * We don't directly use a struct here, to avoid GetPointer, reinterpret_cast, or Read/WriteBlock repeatedly.
     */
    enum HeaderField {
        Magic = 0,
        NameOffset,
        NextCRO,
        PreviousCRO,
        FileSize,
        BssSize,
        FixedSize,
        UnknownZero,
        UnkSegmentTag,
        OnLoadSegmentTag,
        OnExitSegmentTag,
        OnUnresolvedSegmentTag,

        CodeOffset,
        CodeSize,
        DataOffset,
        DataSize,
        ModuleNameOffset,
        ModuleNameSize,
        SegmentTableOffset,
        SegmentNum,

        ExportNamedSymbolTableOffset,
        ExportNamedSymbolNum,
        ExportIndexedSymbolTableOffset,
        ExportIndexedSymbolNum,
        ExportStringsOffset,
        ExportStringsSize,
        ExportTreeTableOffset,
        ExportTreeNum,

        ImportModuleTableOffset,
        ImportModuleNum,
        ExternalPatchTableOffset,
        ExternalPatchNum,
        ImportNamedSymbolTableOffset,
        ImportNamedSymbolNum,
        ImportIndexedSymbolTableOffset,
        ImportIndexedSymbolNum,
        ImportAnonymousSymbolTableOffset,
        ImportAnonymousSymbolNum,
        ImportStringsOffset,
        ImportStringsSize,

        StaticAnonymousSymbolTableOffset,
        StaticAnonymousSymbolNum,
        InternalPatchTableOffset,
        InternalPatchNum,
        StaticPatchTableOffset,
        StaticPatchNum,
        Fix0Barrier,

        Fix3Barrier = ExportNamedSymbolTableOffset,
        Fix2Barrier = ImportModuleTableOffset,
        Fix1Barrier = StaticAnonymousSymbolTableOffset,
    };
    static_assert(Fix0Barrier == (CRO_HEADER_SIZE - CRO_HASH_SIZE) / 4, "CRO Header fields are wrong!");

    enum class SegmentType : u32 {
        Code   = 0,
        ROData = 1,
        Data   = 2,
        BSS    = 3,
    };

    /**
     * Identifies a program location inside of a segment.
     * Required to refer to program locations because individual segments may be relocated independently of each other.
     */
    union SegmentTag {
        u32_le raw;
        BitField<0, 4, u32_le> segment_index;
        BitField<4, 28, u32_le> offset_into_segment;

        SegmentTag() = default;
        SegmentTag(u32 raw_) : raw(raw_) {}
    };

    /// Information of a segment in this module.
    struct SegmentEntry {
        u32_le offset;
        u32_le size;
        SegmentType type;

        static constexpr HeaderField TABLE_OFFSET_FIELD = SegmentTableOffset;
    };
    ASSERT_CRO_STRUCT(SegmentEntry, 12);

    /// Identifies a named symbol exported from this module.
    struct ExportNamedSymbolEntry {
        u32_le name_offset;         // pointing to a substring in ExportStrings
        SegmentTag symbol_position; // to self's segment

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportNamedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportNamedSymbolEntry, 8);

    /// Identifies an indexed symbol exported from this module.
    struct ExportIndexedSymbolEntry {
        SegmentTag symbol_position; // to self's segment

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportIndexedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportIndexedSymbolEntry, 4);

    /// A tree node in the symbol lookup tree.
    struct ExportTreeEntry {
        u16_le test_bit; // bit address into the name to test
        union Child {
            u16_le raw;
            BitField<0, 15, u16_le> next_index;
            BitField<15, 1, u16_le> is_end;
        } left, right;
        u16_le export_table_index; // index of an ExportNamedSymbolEntry

        static constexpr HeaderField TABLE_OFFSET_FIELD = ExportTreeTableOffset;
    };
    ASSERT_CRO_STRUCT(ExportTreeEntry, 8);

    /// Identifies a named symbol imported from other module.
    struct ImportNamedSymbolEntry {
        u32_le name_offset;        // pointing to a substring in ImportStrings
        u32_le patch_batch_offset; // pointing to a patch batch in ExternalPatchTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportNamedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportNamedSymbolEntry, 8);

    /// Identifies an indexed symbol imported from other module.
    struct ImportIndexedSymbolEntry {
        u32_le index;              // index of an opponent's ExportIndexedSymbolEntry
        u32_le patch_batch_offset; // pointing to a patch batch in ExternalPatchTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportIndexedSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportIndexedSymbolEntry, 8);

    /// Identifies an anonymous symbol imported from other module.
    struct ImportAnonymousSymbolEntry {
        SegmentTag symbol_position; // to the opponent's segment
        u32_le patch_batch_offset;  // pointing to a patch batch in ExternalPatchTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportAnonymousSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(ImportAnonymousSymbolEntry, 8);

    /// Information of a referred module and symbols imported from it.
    struct ImportModuleEntry {
        u32_le name_offset;                          // pointing to a substring in ImporStrings
        u32_le import_indexed_symbol_table_offset;   // pointing to a subtable in ImportIndexedSymbolTable
        u32_le import_indexed_symbol_num;
        u32_le import_anonymous_symbol_table_offset; // pointing to a subtable in ImportAnonymousSymbolTable
        u32_le import_anonymous_symbol_num;

        static constexpr HeaderField TABLE_OFFSET_FIELD = ImportModuleTableOffset;

        void GetImportIndexedSymbolEntry(u32 index, ImportIndexedSymbolEntry& entry) {
            Memory::ReadBlock(import_indexed_symbol_table_offset + index * sizeof(ImportIndexedSymbolEntry),
                &entry, sizeof(ImportIndexedSymbolEntry));
        }

        void GetImportAnonymousSymbolEntry(u32 index, ImportAnonymousSymbolEntry& entry) {
            Memory::ReadBlock(import_anonymous_symbol_table_offset + index * sizeof(ImportAnonymousSymbolEntry),
                &entry, sizeof(ImportAnonymousSymbolEntry));
        }
    };
    ASSERT_CRO_STRUCT(ImportModuleEntry, 20);

    enum class PatchType : u8 {
        Nothing                = 0,
        AbsoluteAddress        = 2,
        RelativeAddress        = 3,
        ThumbBranch            = 10,
        ArmBranch              = 28,
        ModifyArmBranch        = 29,
        AbsoluteAddress2       = 38,
        AlignedRelativeAddress = 42,
    };

    struct PatchEntry {
        SegmentTag target_position; // to self's segment as an ExternalPatchEntry; to static module segment as a StaticPatchEntry
        PatchType type;
        u8 is_batch_end;
        u8 is_batch_resolved;       // set at a batch beginning if the batch is resolved
        INSERT_PADDING_BYTES(1);
        u32_le shift;
    };

    /// Identifies a normal cross-module patch.
    struct ExternalPatchEntry : PatchEntry {
        static constexpr HeaderField TABLE_OFFSET_FIELD = ExternalPatchTableOffset;
    };
    ASSERT_CRO_STRUCT(ExternalPatchEntry, 12);

    /// Identifies a special static patch (no game is known using this).
    struct StaticPatchEntry : PatchEntry {
        static constexpr HeaderField TABLE_OFFSET_FIELD = StaticPatchTableOffset;
    };
    ASSERT_CRO_STRUCT(StaticPatchEntry, 12);

    /// Identifies a in-module patch.
    struct InternalPatchEntry {
        SegmentTag target_position; // to self's segment
        PatchType type;
        u8 symbol_segment;
        INSERT_PADDING_BYTES(2);
        u32_le shift;

        static constexpr HeaderField TABLE_OFFSET_FIELD = InternalPatchTableOffset;
    };
    ASSERT_CRO_STRUCT(InternalPatchEntry, 12);

    /// Identifies a special static anonymous symbol (no game is known using this).
    struct StaticAnonymousSymbolEntry {
        SegmentTag symbol_position; // to self's segment
        u32_le patch_batch_offset;  // pointing to a patch batch in StaticPatchTable

        static constexpr HeaderField TABLE_OFFSET_FIELD = StaticAnonymousSymbolTableOffset;
    };
    ASSERT_CRO_STRUCT(StaticAnonymousSymbolEntry, 8);

    static std::array<int, 17> ENTRY_SIZE;
    static std::array<HeaderField, 4> FIX_BARRIERS;

    static constexpr u32 MAGIC_CRO0 = 0x304F5243;
    static constexpr u32 MAGIC_FIXD = 0x44584946;

    VAddr Field(HeaderField field) const {
        return address + CRO_HASH_SIZE + field * 4;
    }

    u32 GetField(HeaderField field) const {
        return Memory::Read32(Field(field));
    }

    void SetField(HeaderField field, u32 value) {
        Memory::Write32(Field(field), value);
    }

    /**
     * Reads an entry in one of module tables.
     * @param index index of the entry
     * @param data where to put the read entry
     * @note the entry type must have the static member TABLE_OFFSET_FIELD
     *       indicating which table the entry is in.
     */
    template <typename T>
    void GetEntry(std::size_t index, T& data) const {
        Memory::ReadBlock(GetField(T::TABLE_OFFSET_FIELD) + index * sizeof(T), &data, sizeof(T));
    }

    /**
     * Writes an entry to one of module tables.
     * @param index index of the entry
     * @param data the entry data to write
     * @note the entry type must have the static member TABLE_OFFSET_FIELD
     *       indicating which table the entry is in.
     */
    template <typename T>
    void SetEntry(std::size_t index, const T& data) {
        Memory::WriteBlock(GetField(T::TABLE_OFFSET_FIELD) + index * sizeof(T), &data, sizeof(T));
    }

    /**
     * Converts a segment tag to virtual address in this module.
     * @param segment_tag the segment tag to convert
     * @returns VAddr the virtual address the segment tag points to; 0 if invalid.
     */
    VAddr SegmentTagToAddress(SegmentTag segment_tag) const {
        u32 segment_num = GetField(SegmentNum);

        if (segment_tag.segment_index >= segment_num)
            return 0;

        SegmentEntry entry;
        GetEntry(segment_tag.segment_index, entry);

        if (segment_tag.offset_into_segment >= entry.size)
            return 0;

        return entry.offset + segment_tag.offset_into_segment;
    }

public:
    explicit CROHelper(VAddr cro_address) : address(cro_address) {
    }

    std::string ModuleName() const {
        return Memory::ReadCString(GetField(ModuleNameOffset), GetField(ModuleNameSize));
    }

    u32 GetFileSize() const {
        return GetField(FileSize);
    }

};

std::array<int, 17> CROHelper::ENTRY_SIZE {{
    1, // code
    1, // data
    1, // module name
    sizeof(SegmentEntry),
    sizeof(ExportNamedSymbolEntry),
    sizeof(ExportIndexedSymbolEntry),
    1, // export strings
    sizeof(ExportTreeEntry),
    sizeof(ImportModuleEntry),
    sizeof(ExternalPatchEntry),
    sizeof(ImportNamedSymbolEntry),
    sizeof(ImportIndexedSymbolEntry),
    sizeof(ImportAnonymousSymbolEntry),
    1, // import strings
    sizeof(StaticAnonymousSymbolEntry),
    sizeof(InternalPatchEntry),
    sizeof(StaticPatchEntry)
}};

std::array<CROHelper::HeaderField, 4> CROHelper::FIX_BARRIERS {{
    Fix0Barrier,
    Fix1Barrier,
    Fix2Barrier,
    Fix3Barrier
}};

/**
 * LDR_RO::Initialize service function
 *  Inputs:
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Process memory address where the CRS will be mapped
 *      4 : Value, must be zero
 *      5 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crs_buffer_ptr = cmd_buff[1];
    u32 crs_size       = cmd_buff[2];
    u32 address        = cmd_buff[3];
    u32 value          = cmd_buff[4];
    u32 process        = cmd_buff[5];

    if (value != 0) {
        LOG_ERROR(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    // TODO(purpasmart96): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, address=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, address, value, process);
}

/**
 * LDR_RO::LoadCRR service function
 *  Inputs:
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Value, must be zero
 *      4 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void LoadCRR(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crs_buffer_ptr = cmd_buff[1];
    u32 crs_size       = cmd_buff[2];
    u32 value          = cmd_buff[3];
    u32 process        = cmd_buff[4];

    if (value != 0) {
        LOG_ERROR(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    // TODO(purpasmart96): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, value, process);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C2, Initialize,            "Initialize"},
    {0x00020082, LoadCRR,               "LoadCRR"},
    {0x00030042, nullptr,               "UnloadCCR"},
    {0x000402C2, nullptr,               "LoadExeCRO"},
    {0x000500C2, nullptr,               "LoadCROSymbols"},
    {0x00060042, nullptr,               "CRO_Load?"},
    {0x00070042, nullptr,               "LoadCROSymbols"},
    {0x00080042, nullptr,               "Shutdown"},
    {0x000902C2, nullptr,               "LoadExeCRO_New?"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
