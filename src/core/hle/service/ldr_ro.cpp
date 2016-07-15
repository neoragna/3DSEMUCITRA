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

    VAddr Next() const {
        return GetField(NextCRO);
    }

    VAddr Previous() const {
        return GetField(PreviousCRO);
    }

    void SetNext(VAddr next) {
        SetField(NextCRO, next);
    }

    void SetPrevious(VAddr previous) {
        SetField(PreviousCRO, previous);
    }

    /**
     * A helper function iterating over all registered auto-link modules, including the static module.
     * @param crs_address the virtual address of the static module
     * @param func a function object to operate on a module. It accepts one parameter
     *        CROHelper and returns ResultVal<bool>. It should return true to continue the iteration,
     *        false to stop the iteration, or an error code (which will also stop the iteration).
     * @returns ResultCode indicating the result of the operation, RESULT_SUCCESS if all iteration success,
     *         otherwise error code of the last iteration.
     */
    template <typename FunctionObject>
    static ResultCode ForEachAutoLinkCRO(VAddr crs_address, FunctionObject func) {
        VAddr current = crs_address;
        while (current) {
            CROHelper cro(current);
            CASCADE_RESULT(bool next, func(cro));
            if (!next)
                break;
            current = cro.Next();
        }
        return RESULT_SUCCESS;
    }

    /**
     * Applies a patch
     * @param target_address where to apply the patch
     * @param patch_type the type of the patch
     * @param shift address shift apply to the patched symbol
     * @param symbol_address the symbol address to be patched with
     * @param target_future_address the future address of the target.
     *        Usually equals to target_address, but will be different for a target in .data segment
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ApplyPatch(VAddr target_address, PatchType patch_type, u32 shift, u32 symbol_address, u32 target_future_address) {
        switch (patch_type) {
        case PatchType::Nothing:
            break;
        case PatchType::AbsoluteAddress:
        case PatchType::AbsoluteAddress2:
            Memory::Write32(target_address, symbol_address + shift);
            break;
        case PatchType::RelativeAddress:
            Memory::Write32(target_address, symbol_address + shift - target_future_address);
            break;
        case PatchType::ThumbBranch:
        case PatchType::ArmBranch:
        case PatchType::ModifyArmBranch:
        case PatchType::AlignedRelativeAddress:
            // TODO(wwylele): implement other types
            UNIMPLEMENTED();
            break;
        default:
            return CROFormatError(0x22);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Clears a patch to zero
     * @param target_address where to apply the patch
     * @param patch_type the type of the patch
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ClearPatch(VAddr target_address, PatchType patch_type) {
        switch (patch_type) {
        case PatchType::Nothing:
            break;
        case PatchType::AbsoluteAddress:
        case PatchType::AbsoluteAddress2:
        case PatchType::RelativeAddress:
            Memory::Write32(target_address, 0);
            break;
        case PatchType::ThumbBranch:
        case PatchType::ArmBranch:
        case PatchType::ModifyArmBranch:
        case PatchType::AlignedRelativeAddress:
            // TODO(wwylele): implement other types
            UNIMPLEMENTED();
            break;
        default:
            return CROFormatError(0x22);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Applies or resets a batch of patch
     * @param batch the virtual address of the first patch in the batch
     * @param symbol_address the symbol address to be patched with
     * @param reset false to set the batch to resolved state, true to reset the batch to unresolved state
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ApplyPatchBatch(VAddr batch, u32 symbol_address, bool reset = false) {
        if (symbol_address == 0 && !reset)
            return CROFormatError(0x10);

        VAddr patch_address = batch;
        while (true) {
            PatchEntry patch;
            Memory::ReadBlock(patch_address, &patch, sizeof(PatchEntry));

            VAddr patch_target = SegmentTagToAddress(patch.target_position);
            if (patch_target == 0) {
                return CROFormatError(0x12);
            }

            ResultCode result = ApplyPatch(patch_target, patch.type, patch.shift, symbol_address, patch_target);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying patch %08X", result.raw);
                return result;
            }

            if (patch.is_batch_end)
                break;

            patch_address += sizeof(PatchEntry);
        }

        PatchEntry patch;
        Memory::ReadBlock(batch, &patch, sizeof(PatchEntry));
        patch.is_batch_resolved = reset ? 0 : 1;
        Memory::WriteBlock(batch, &patch, sizeof(PatchEntry));
        return RESULT_SUCCESS;
    }

    /**
     * Finds an exported named symbol in this module.
     * @param name the name of the symbol to find
     * @return VAddr the virtual address of the symbol; 0 if not found.
     */
    VAddr FindExportNamedSymbol(const std::string& name) const {
        if (!GetField(ExportTreeNum))
            return 0;

        std::size_t len = name.size();
        ExportTreeEntry entry;
        GetEntry(0, entry);
        ExportTreeEntry::Child next;
        next.raw = entry.left.raw;
        u32 found_id;

        while (true) {
            GetEntry(next.next_index, entry);

            if (next.is_end) {
                found_id = entry.export_table_index;
                break;
            }

            u16 test_byte = entry.test_bit >> 3;
            u16 test_bit_in_byte = entry.test_bit & 7;

            if (test_byte >= len) {
                next.raw = entry.left.raw;
            } else if((name[test_byte] >> test_bit_in_byte) & 1) {
                next.raw = entry.right.raw;
            } else {
                next.raw = entry.left.raw;
            }
        }

        u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);

        if (found_id >= export_named_symbol_num)
            return 0;

        u32 export_strings_size = GetField(ExportStringsSize);
        ExportNamedSymbolEntry symbol_entry;
        GetEntry(found_id, symbol_entry);

        if (Memory::ReadCString(symbol_entry.name_offset, export_strings_size) != name)
            return 0;

        return SegmentTagToAddress(symbol_entry.symbol_position);
    }

    /**
     * Rebases offsets in module header according to module address.
     * @param cro_size the size of the CRO file
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseHeader(u32 cro_size) {
        ResultCode error = CROFormatError(0x11);

        // verifies magic
        if (GetField(Magic) != MAGIC_CRO0)
            return error;

        // verifies not registered
        if (GetField(NextCRO) || GetField(PreviousCRO))
            return error;

        // This seems to be a hard limit set by the RO module
        if (GetField(FileSize) > 0x10000000 || GetField(BssSize) > 0x10000000)
            return error;

        // verifies not fixed
        if (GetField(FixedSize))
            return error;

        if (GetField(CodeOffset) < CRO_HEADER_SIZE)
            return error;

        // verifies that all offsets are in the correct order
        constexpr std::array<HeaderField, 18> OFFSET_ORDER = {{
            CodeOffset,
            ModuleNameOffset,
            SegmentTableOffset,
            ExportNamedSymbolTableOffset,
            ExportTreeTableOffset,
            ExportIndexedSymbolTableOffset,
            ExportStringsOffset,
            ImportModuleTableOffset,
            ExternalPatchTableOffset,
            ImportNamedSymbolTableOffset,
            ImportIndexedSymbolTableOffset,
            ImportAnonymousSymbolTableOffset,
            ImportStringsOffset,
            StaticAnonymousSymbolTableOffset,
            InternalPatchTableOffset,
            StaticPatchTableOffset,
            DataOffset,
            FileSize
        }};

        u32 prev_offset = GetField(OFFSET_ORDER[0]);
        u32 cur_offset;
        for (std::size_t i = 1; i < OFFSET_ORDER.size(); ++i) {
            cur_offset = GetField(OFFSET_ORDER[i]);
            if (cur_offset < prev_offset)
                return error;
            prev_offset = cur_offset;
        }

        // rebases offsets
        u32 offset = GetField(NameOffset);
        if (offset)
            SetField(NameOffset, offset + address);

        for (int field = CodeOffset; field < Fix0Barrier; field += 2) {
            HeaderField header_field = static_cast<HeaderField>(field);
            offset = GetField(header_field);
            if (offset)
                SetField(header_field, offset + address);
        }

        // verifies everything is not beyond the buffer
        u32 file_end = address + cro_size;
        for (int field = CodeOffset, i = 0; field < Fix0Barrier; field += 2, ++i) {
            HeaderField offset_field = static_cast<HeaderField>(field);
            HeaderField size_field = static_cast<HeaderField>(field + 1);
            if (GetField(offset_field) + GetField(size_field) * ENTRY_SIZE[i] > file_end)
                return error;
        }

        return RESULT_SUCCESS;
    }

    /**
     * Verifies a string matching a predicted size (i.e. terminated by 0) if it is not empty
     * @param address the virtual address of the string
     * @param size the size of the string, including the terminating 0
     * @returns ResultCode RESULT_SUCCESS if the size matches, otherwise error code.
     */
    static ResultCode VerifyString(VAddr address, u32 size) {
        if (size) {
            if (Memory::Read8(address + size - 1) != 0)
                return CROFormatError(0x0B);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Rebases offsets in segment table according to module address.
     * @param cro_size the size of the CRO file
     * @param data_segment_address the buffer address for .data segment
     * @param data_segment_size the buffer size for .data segment
     * @param bss_segment_address the buffer address for .bss segment
     * @param bss_segment_size the buffer size for .bss segment
     * @returns ResultVal<u32> with the previous data segment offset before rebasing.
     */
    ResultVal<u32> RebaseSegmentTable(u32 cro_size,
        VAddr data_segment_address, u32 data_segment_size,
        VAddr bss_segment_address, u32 bss_segment_size) {
        u32 prev_data_segment = 0;
        u32 segment_num = GetField(SegmentNum);
        for (u32 i = 0; i < segment_num; ++i) {
            SegmentEntry segment;
            GetEntry(i, segment);
            if (segment.type == SegmentType::Data) {
                if (segment.size) {
                    if (segment.size > data_segment_size)
                        return ERROR_BUFFER_TOO_SMALL;
                    prev_data_segment = segment.offset;
                    segment.offset = data_segment_address;
                }
            } else if (segment.type == SegmentType::BSS) {
                if (segment.size) {
                    if (segment.size > bss_segment_size)
                        return ERROR_BUFFER_TOO_SMALL;
                    segment.offset = bss_segment_address;
                }
            } else if (segment.offset) {
                segment.offset += address;
                if (segment.offset > address + cro_size)
                    return CROFormatError(0x19);
            }
            SetEntry(i, segment);
        }
        return MakeResult<u32>(prev_data_segment);
    }

    /**
     * Rebases offsets in exported named symbol table according to module address.
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseExportNamedSymbolTable() {
        VAddr export_strings_offset = GetField(ExportStringsOffset);
        VAddr export_strings_end = export_strings_offset + GetField(ExportStringsSize);

        u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);
        for (u32 i = 0; i < export_named_symbol_num; ++i) {
            ExportNamedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset += address;
                if (entry.name_offset < export_strings_offset
                    || entry.name_offset >= export_strings_end) {
                    return CROFormatError(0x11);
                }
            }

            SetEntry(i, entry);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Verifies indeces in export tree table.
     * @returns ResultCode RESULT_SUCCESS if all indeces are verified as valid, otherwise error code.
     */
    ResultCode VerifyExportTreeTable() const {
        u32 tree_num = GetField(ExportTreeNum);
        for (u32 i = 0; i < tree_num; ++i) {
            ExportTreeEntry entry;
            GetEntry(i, entry);

            if (entry.left.next_index >= tree_num || entry.right.next_index >= tree_num) {
                return CROFormatError(0x11);
            }
        }
        return RESULT_SUCCESS;
    }

    /**
     * Rebases offsets in exported module table according to module address.
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseImportModuleTable() {
        VAddr import_strings_offset = GetField(ImportStringsOffset);
        VAddr import_strings_end = import_strings_offset + GetField(ImportStringsSize);
        VAddr import_indexed_symbol_table_offset = GetField(ImportIndexedSymbolTableOffset);
        VAddr index_import_table_end = import_indexed_symbol_table_offset + GetField(ImportIndexedSymbolNum) * sizeof(ImportIndexedSymbolEntry);
        VAddr import_anonymous_symbol_table_offset = GetField(ImportAnonymousSymbolTableOffset);
        VAddr offset_import_table_end = import_anonymous_symbol_table_offset + GetField(ImportAnonymousSymbolNum) * sizeof(ImportAnonymousSymbolEntry);

        u32 module_num = GetField(ImportModuleNum);
        for (u32 i = 0; i < module_num; ++i) {
            ImportModuleEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset += address;
                if (entry.name_offset < import_strings_offset
                    || entry.name_offset >= import_strings_end) {
                    return CROFormatError(0x18);
                }
            }

            if (entry.import_indexed_symbol_table_offset) {
                entry.import_indexed_symbol_table_offset += address;
                if (entry.import_indexed_symbol_table_offset < import_indexed_symbol_table_offset
                    || entry.import_indexed_symbol_table_offset > index_import_table_end) {
                    return CROFormatError(0x18);
                }
            }

            if (entry.import_anonymous_symbol_table_offset) {
                entry.import_anonymous_symbol_table_offset += address;
                if (entry.import_anonymous_symbol_table_offset < import_anonymous_symbol_table_offset
                    || entry.import_anonymous_symbol_table_offset > offset_import_table_end) {
                    return CROFormatError(0x18);
                }
            }

            SetEntry(i, entry);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Rebases offsets in imported named symbol table according to module address.
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseImportNamedSymbolTable() {
        VAddr import_strings_offset = GetField(ImportStringsOffset);
        VAddr import_strings_end = import_strings_offset + GetField(ImportStringsSize);
        VAddr external_patch_table_offset = GetField(ExternalPatchTableOffset);
        VAddr external_patch_table_end = external_patch_table_offset + GetField(ExternalPatchNum) * sizeof(ExternalPatchEntry);

        u32 num = GetField(ImportNamedSymbolNum);
        for (u32 i = 0; i < num ; ++i) {
            ImportNamedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset += address;
                if (entry.name_offset < import_strings_offset
                    || entry.name_offset >= import_strings_end) {
                    return CROFormatError(0x1B);
                }
            }

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset += address;
                if (entry.patch_batch_offset < external_patch_table_offset
                    || entry.patch_batch_offset > external_patch_table_end) {
                    return CROFormatError(0x1B);
                }
            }

            SetEntry(i, entry);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Rebases offsets in imported indexed symbol table according to module address.
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseImportIndexedSymbolTable() {
        VAddr external_patch_table_offset = GetField(ExternalPatchTableOffset);
        VAddr external_patch_table_end = external_patch_table_offset + GetField(ExternalPatchNum) * sizeof(ExternalPatchEntry);

        u32 num = GetField(ImportIndexedSymbolNum);
        for (u32 i = 0; i < num ; ++i) {
            ImportIndexedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset += address;
                if (entry.patch_batch_offset < external_patch_table_offset
                    || entry.patch_batch_offset > external_patch_table_end) {
                    return CROFormatError(0x14);
                }
            }

            SetEntry(i, entry);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Rebases offsets in imported anonymous symbol table according to module address.
     * @returns ResultCode RESULT_SUCCESS if all offsets are verified as valid, otherwise error code.
     */
    ResultCode RebaseImportAnonymousSymbolTable() {
        VAddr external_patch_table_offset = GetField(ExternalPatchTableOffset);
        VAddr external_patch_table_end = external_patch_table_offset + GetField(ExternalPatchNum) * sizeof(ExternalPatchEntry);

        u32 num = GetField(ImportAnonymousSymbolNum);
        for (u32 i = 0; i < num ; ++i) {
            ImportAnonymousSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset += address;
                if (entry.patch_batch_offset < external_patch_table_offset
                    || entry.patch_batch_offset > external_patch_table_end) {
                    return CROFormatError(0x17);
                }
            }

            SetEntry(i, entry);
        }
        return RESULT_SUCCESS;
    }

    /**
     * Resets all external patches to unresolved state.
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ResetExternalPatches() {
        u32 unresolved_symbol = SegmentTagToAddress(GetField(OnUnresolvedSegmentTag));
        u32 external_patch_num = GetField(ExternalPatchNum);
        ExternalPatchEntry patch;

        // Verifies that the last patch is the end of a batch
        GetEntry(external_patch_num - 1, patch);
        if (!patch.is_batch_end) {
            return CROFormatError(0x12);
        }

        bool batch_begin = true;
        for (u32 i = 0; i < external_patch_num; ++i) {
            GetEntry(i, patch);
            VAddr patch_target = SegmentTagToAddress(patch.target_position);

            if (patch_target == 0) {
                return CROFormatError(0x12);
            }

            ResultCode result = ApplyPatch(patch_target, patch.type, patch.shift, unresolved_symbol, patch_target);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying patch %08X", result.raw);
                return result;
            }

            if (batch_begin) {
                // resets to unresolved state
                patch.is_batch_resolved = 0;
                SetEntry(i, patch);
            }

            // if current is an end, then the next is a beginning
            batch_begin = patch.is_batch_end != 0;
        }

        return RESULT_SUCCESS;
    }

    /**
     * Applies all static anonymous symbol to the static module.
     * @param crs_address the virtual address of the static module
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ApplyStaticAnonymousSymbolToCRS(VAddr crs_address) {
        VAddr static_patch_table_offset = GetField(StaticPatchTableOffset);
        VAddr static_patch_table_end = static_patch_table_offset + GetField(StaticPatchNum) * sizeof(StaticPatchEntry);

        CROHelper crs(crs_address);
        u32 offset_export_num = GetField(StaticAnonymousSymbolNum);
        LOG_INFO(Service_LDR, "CRO \"%s\" exports %d static anonymous symbols", ModuleName().data(), offset_export_num);
        for (u32 i = 0; i < offset_export_num; ++i) {
            StaticAnonymousSymbolEntry entry;
            GetEntry(i, entry);
            u32 batch_address = entry.patch_batch_offset + address;

            if (batch_address < static_patch_table_offset
                || batch_address > static_patch_table_end) {
                return CROFormatError(0x16);
            }

            u32 symbol_address = SegmentTagToAddress(entry.symbol_position);
            LOG_TRACE(Service_LDR, "CRO \"%s\" exports 0x%08X to the static module", ModuleName().data(), symbol_address);
            ResultCode result = crs.ApplyPatchBatch(batch_address, symbol_address);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying patch batch %08X", result.raw);
                return result;
            }
        }
        return RESULT_SUCCESS;
    }

    /**
     * Applies all internal patches to the module itself.
     * @param old_data_segment_address the virtual address of data segment in CRO buffer
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ApplyInternalPatches(u32 old_data_segment_address) {
        u32 segment_num = GetField(SegmentNum);
        u32 internal_patch_num = GetField(InternalPatchNum);
        for (u32 i = 0; i < internal_patch_num; ++i) {
            InternalPatchEntry patch;
            GetEntry(i, patch);
            VAddr target_addressB = SegmentTagToAddress(patch.target_position);
            if (target_addressB == 0) {
                return CROFormatError(0x15);
            }

            VAddr target_address;
            SegmentEntry target_segment;
            GetEntry(patch.target_position.segment_index, target_segment);

            if (target_segment.type == SegmentType::Data) {
                // If the patch is to the .data segment, we need to patch it in the old buffer
                target_address = old_data_segment_address + patch.target_position.offset_into_segment;
            } else {
                target_address = target_addressB;
            }

            if (patch.symbol_segment >= segment_num) {
                return CROFormatError(0x15);
            }

            SegmentEntry symbol_segment;
            GetEntry(patch.symbol_segment, symbol_segment);
            LOG_TRACE(Service_LDR, "Internally patches 0x%08X with 0x%08X", target_address, symbol_segment.offset);
            ResultCode result = ApplyPatch(target_address, patch.type, patch.shift, symbol_segment.offset, target_addressB);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying patch %08X", result.raw);
                return result;
            }
        }
        return RESULT_SUCCESS;
    }

    /**
     * Clears all internal patches to zero.
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ClearInternalPatches() {
        u32 internal_patch_num = GetField(InternalPatchNum);
        for (u32 i = 0; i < internal_patch_num; ++i) {
            InternalPatchEntry patch;
            GetEntry(i, patch);
            VAddr target_address = SegmentTagToAddress(patch.target_position);

            if (target_address == 0) {
                return CROFormatError(0x15);
            }

            ResultCode result = ClearPatch(target_address, patch.type);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error clearing patch %08X", result.raw);
                return result;
            }
        }
        return RESULT_SUCCESS;
    }

    /// Unrebases offsets in imported anonymous symbol table
    void UnrebaseImportAnonymousSymbolTable() {
        u32 num = GetField(ImportAnonymousSymbolNum);
        for (u32 i = 0; i < num; ++i) {
            ImportAnonymousSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset -= address;
            }

            SetEntry(i, entry);
        }
    }

    /// Unrebases offsets in imported indexed symbol table
    void UnrebaseImportIndexedSymbolTable() {
        u32 num = GetField(ImportIndexedSymbolNum);
        for (u32 i = 0; i < num; ++i) {
            ImportIndexedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset -= address;
            }

            SetEntry(i, entry);
        }
    }

    /// Unrebases offsets in imported named symbol table
    void UnrebaseImportNamedSymbolTable() {
        u32 num = GetField(ImportNamedSymbolNum);
        for (u32 i = 0; i < num; ++i) {
            ImportNamedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset -= address;
            }

            if (entry.patch_batch_offset) {
                entry.patch_batch_offset -= address;
            }

            SetEntry(i, entry);
        }
    }

    /// Unrebases offsets in imported module table
    void UnrebaseImportModuleTable() {
        u32 module_num = GetField(ImportModuleNum);
        for (u32 i = 0; i < module_num; ++i) {
            ImportModuleEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset -= address;
            }

            if (entry.import_indexed_symbol_table_offset) {
                entry.import_indexed_symbol_table_offset -= address;
            }

            if (entry.import_anonymous_symbol_table_offset) {
                entry.import_anonymous_symbol_table_offset -= address;
            }

            SetEntry(i, entry);
        }
    }

    /// Unrebases offsets in exported named symbol table
    void UnrebaseExportNamedSymbolTable() {
        u32 export_named_symbol_num = GetField(ExportNamedSymbolNum);
        for (u32 i = 0; i < export_named_symbol_num; ++i) {
            ExportNamedSymbolEntry entry;
            GetEntry(i, entry);

            if (entry.name_offset) {
                entry.name_offset -= address;
            }

            SetEntry(i, entry);
        }
    }

    /// Unrebases offsets in segment table
    void UnrebaseSegmentTable() {
        u32 segment_num = GetField(SegmentNum);
        for (u32 i = 0; i < segment_num; ++i) {
            SegmentEntry segment;
            GetEntry(i, segment);

            if (segment.type == SegmentType::BSS) {
                segment.offset = 0;
            } else if (segment.offset) {
                segment.offset -= address;
            }

            SetEntry(i, segment);
        }
    }

    /// Unrebases offsets in module header
    void UnrebaseHeader() {
        u32 offset = GetField(NameOffset);
        if (offset)
            SetField(NameOffset, offset - address);

        for (int field = CodeOffset; field < Fix0Barrier; field += 2) {
            HeaderField header_field = static_cast<HeaderField>(field);
            offset = GetField(header_field);
            if (offset)
                SetField(header_field, offset - address);
        }
    }

    /**
     * Resolves the exit function in this module
     * @param crs_address the virtual address of the static module.
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode ApplyExitPatches(VAddr crs_address) {
        u32 import_strings_size = GetField(ImportStringsSize);
        u32 symbol_import_num = GetField(ImportNamedSymbolNum);
        for (u32 i = 0; i < symbol_import_num; ++i) {
            ImportNamedSymbolEntry entry;
            GetEntry(i, entry);
            VAddr patch_addr = entry.patch_batch_offset;
            ExternalPatchEntry patch_entry;
            Memory::ReadBlock(patch_addr, &patch_entry, sizeof(ExternalPatchEntry));

            if (Memory::ReadCString(entry.name_offset, import_strings_size) == "__aeabi_atexit"){
                ResultCode result = ForEachAutoLinkCRO(crs_address, [&](CROHelper source) -> ResultVal<bool> {
                    u32 symbol_address = source.FindExportNamedSymbol("nnroAeabiAtexit_");

                    if (symbol_address) {
                        LOG_DEBUG(Service_LDR, "CRO \"%s\" import exit function from \"%s\"",
                            ModuleName().data(), source.ModuleName().data());

                        ResultCode result = ApplyPatchBatch(patch_addr, symbol_address);
                        if (result.IsError()) {
                            LOG_ERROR(Service_LDR, "Error applying patch batch %08X", result.raw);
                            return result;
                        }

                        return MakeResult<bool>(false);
                    }

                    return MakeResult<bool>(true);
                });
                if (result.IsError()) {
                    LOG_ERROR(Service_LDR, "Error applying exit patch %08X", result.raw);
                    return result;
                }
            }
        }
        return RESULT_SUCCESS;
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

    /**
     * Rebases the module according to its address.
     * @param crs_address the virtual address of the static module
     * @param cro_size the size of the CRO file
     * @param data_segment_address buffer address for .data segment
     * @param data_segment_size the buffer size for .data segment
     * @param bss_segment_address the buffer address for .bss segment
     * @param bss_segment_size the buffer size for .bss segment
     * @param is_crs true if the module itself is the static module
     * @returns ResultCode RESULT_SUCCESS on success, otherwise error code.
     */
    ResultCode Rebase(VAddr crs_address, u32 cro_size,
        VAddr data_segment_addresss, u32 data_segment_size,
        VAddr bss_segment_address, u32 bss_segment_size, bool is_crs) {
        ResultCode result = RebaseHeader(cro_size);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing header %08X", result.raw);
            return result;
        }

        result = VerifyString(GetField(ModuleNameOffset), GetField(ModuleNameSize));
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error verifying module name %08X", result.raw);
            return result;
        }

        u32 prev_data_segment_address = 0;
        if (!is_crs) {
            auto result_val = RebaseSegmentTable(cro_size,
                data_segment_addresss, data_segment_size,
                bss_segment_address, bss_segment_size);
            if (result_val.Failed()) {
                LOG_ERROR(Service_LDR, "Error rebasing segment table %08X", result_val.Code().raw);
                return result_val.Code();
            }
            prev_data_segment_address = *result_val;
        }
        prev_data_segment_address += address;

        result = RebaseExportNamedSymbolTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing symbol export table %08X", result.raw);
            return result;
        }

        result = VerifyExportTreeTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error verifying export tree %08X", result.raw);
            return result;
        }

        result = VerifyString(GetField(ExportStringsOffset), GetField(ExportStringsSize));
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error verifying export strings %08X", result.raw);
            return result;
        }

        result = RebaseImportModuleTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing object table %08X", result.raw);
            return result;
        }

        result = ResetExternalPatches();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error resetting all external patches %08X", result.raw);
            return result;
        }

        result = RebaseImportNamedSymbolTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing symbol import table %08X", result.raw);
            return result;
        }

        result = RebaseImportIndexedSymbolTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing index import table %08X", result.raw);
            return result;
        }

        result = RebaseImportAnonymousSymbolTable();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error rebasing offset import table %08X", result.raw);
            return result;
        }

        result = VerifyString(GetField(ImportStringsOffset), GetField(ImportStringsSize));
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error verifying import strings %08X", result.raw);
            return result;
        }

        if (!is_crs) {
            result = ApplyStaticAnonymousSymbolToCRS(crs_address);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying offset export to CRS %08X", result.raw);
                return result;
            }
        }

        result = ApplyInternalPatches(prev_data_segment_address);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error applying internal patches %08X", result.raw);
            return result;
        }

        if (!is_crs) {
            result = ApplyExitPatches(crs_address);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error applying exit patches %08X", result.raw);
                return result;
            }
        }

        return RESULT_SUCCESS;
    }

    /**
     * Unrebases the module.
     * @param is_crs true if the module itself is the static module
     */
    void Unrebase(bool is_crs) {
        UnrebaseImportAnonymousSymbolTable();
        UnrebaseImportIndexedSymbolTable();
        UnrebaseImportNamedSymbolTable();
        UnrebaseImportModuleTable();
        UnrebaseExportNamedSymbolTable();

        if (!is_crs)
            UnrebaseSegmentTable();

        SetNext(0);
        SetPrevious(0);

        SetField(FixedSize, 0);

        UnrebaseHeader();
    }

    void InitCRS() {
        SetNext(0);
        SetPrevious(0);
    }

    /**
     * Registers this module and adds it to the module list.
     * @param crs_address the virtual address of the static module
     * @auto_link whether to register as an auto link module
     */
    void Register(VAddr crs_address, bool auto_link) {
        CROHelper crs(crs_address);
        CROHelper head(auto_link ? crs.Next() : crs.Previous());

        if (head.address) {
            // there are already CROs registered
            // register as the new tail
            CROHelper tail(head.Previous());

            // link with the old tail
            ASSERT(tail.Next() == 0);
            SetPrevious(tail.address);
            tail.SetNext(address);

            // set previous of the head pointing to the new tail
            head.SetPrevious(address);
        } else {
            // register as the first CRO
            // set previous to self as tail
            SetPrevious(address);

            // set self as head
            if (auto_link)
                crs.SetNext(address);
            else
                crs.SetPrevious(address);
        }

        // the new one is the tail
        SetNext(0);
    }

    /**
     * Unregisters this module and removes from the module list.
     * @param crs_address the virtual address of the static module
     */
    void Unregister(VAddr crs_address) {
        CROHelper crs(crs_address);
        CROHelper next_head(crs.Next()), previous_head(crs.Previous());
        CROHelper next(Next()), previous(Previous());

        if (address == next_head.address || address == previous_head.address) {
            // removing head
            if (next.address) {
                // the next is new head
                // let its previous point to the tail
                next.SetPrevious(previous.address);
            }

            // set new head
            if (address == previous_head.address) {
                crs.SetPrevious(next.address);
            } else {
                crs.SetNext(next.address);
            }
        } else if (next.address) {
            // link previous and next
            previous.SetNext(next.address);
            next.SetPrevious(previous.address);
        } else {
            // removing tail
            // set previous as new tail
            previous.SetNext(0);

            // let head's previous point to the new tail
            if (next_head.address && next_head.Previous() == address) {
                next_head.SetPrevious(previous.address);
            } else if (previous_head.address && previous_head.Previous() == address) {
                previous_head.SetPrevious(previous.address);
            } else {
                UNREACHABLE();
            }
        }

        // unlink self
        SetNext(0);
        SetPrevious(0);
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
