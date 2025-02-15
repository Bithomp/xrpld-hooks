#ifndef APPLY_HOOK_INCLUDED
#define APPLY_HOOK_INCLUDED
#include <ripple/basics/Blob.h>
#include <ripple/protocol/TER.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/protocol/SField.h>
#include <queue>
#include <optional>
#include <any>
#include <memory>
#include <vector>
#include <ripple/protocol/digest.h>
#include "common/value.h"
#include "vm/configure.h"
#include "vm/vm.h"
#include "common/errcode.h"
#include "runtime/hostfunc.h"
#include "runtime/importobj.h"

namespace hook {
    struct HookContext;
    struct HookResult;
    bool isEmittedTxn(ripple::STTx const& tx);
}

namespace hook_api {

#define TER_TO_HOOK_RETURN_CODE(x)\
    (((TERtoInt(x)) << 16)*-1)


// for debugging if you want a lot of output change these to if (1)
#define DBG_PRINTF if (0) printf
#define DBG_FPRINTF if (0) fprintf

    namespace keylet_code {
    enum keylet_code : uint32_t {
        HOOK = 1,
        HOOK_STATE = 2,
        ACCOUNT = 3,
        AMENDMENTS = 4,
        CHILD = 5,
        SKIP = 6,
        FEES = 7,
        NEGATIVE_UNL = 8,
        LINE = 9,
        OFFER = 10,
        QUALITY = 11,
        EMITTED_DIR = 12,
        TICKET = 13,
        SIGNERS = 14,
        CHECK = 15,
        DEPOSIT_PREAUTH = 16,
        UNCHECKED = 17,
        OWNER_DIR = 18,
        PAGE = 19,
        ESCROW = 20,
        PAYCHAN = 21,
        EMITTED = 22
    };
    }

    namespace compare_mode {
    enum compare_mode : uint32_t {
        EQUAL = 1,
        LESS = 2,
        GREATER = 4
    };
    }

    enum hook_return_code : int64_t {
        SUCCESS = 0,                    // return codes > 0 are reserved for hook apis to return "success"
        OUT_OF_BOUNDS = -1,             // could not read or write to a pointer to provided by hook
        INTERNAL_ERROR = -2,            // eg directory is corrupt
        TOO_BIG = -3,                   // something you tried to store was too big
        TOO_SMALL = -4,                 // something you tried to store or provide was too small
        DOESNT_EXIST = -5,              // something you requested wasn't found
        NO_FREE_SLOTS = -6,             // when trying to load an object there is a maximum of 255 slots
        INVALID_ARGUMENT = -7,          // self explanatory
        ALREADY_SET = -8,               // returned when a one-time parameter was already set by the hook
        PREREQUISITE_NOT_MET = -9,      // returned if a required param wasn't set, before calling
        FEE_TOO_LARGE = -10,            // returned if the attempted operation would result in an absurd fee
        EMISSION_FAILURE = -11,         // returned if an emitted tx was not accepted by rippled
        TOO_MANY_NONCES = -12,          // a hook has a maximum of 256 nonces
        TOO_MANY_EMITTED_TXN = -13,     // a hook has emitted more than its stated number of emitted txn
        NOT_IMPLEMENTED = -14,          // an api was called that is reserved for a future version
        INVALID_ACCOUNT = -15,          // an api expected an account id but got something else
        GUARD_VIOLATION = -16,          // a guarded loop or function iterated over its maximum
        INVALID_FIELD = -17,            // the field requested is returning sfInvalid
        PARSE_ERROR = -18,              // hook asked hookapi to parse something the contents of which was invalid
        RC_ROLLBACK = -19,              // hook should terminate due to a rollback() call
        RC_ACCEPT = -20,                // hook should temrinate due to an accept() call
        NO_SUCH_KEYLET = -21,           // invalid keylet or keylet type
        NOT_AN_ARRAY = -22,             // if a count of an sle is requested but its not STI_ARRAY
        NOT_AN_OBJECT = -23,            // if a subfield is requested from something that isn't an object
        INVALID_FLOAT = -10024,         // specially selected value that will never be a valid exponent
        DIVISION_BY_ZERO = -25,
        MANTISSA_OVERSIZED = -26,
        MANTISSA_UNDERSIZED = -27,
        EXPONENT_OVERSIZED = -28,
        EXPONENT_UNDERSIZED = -29,
        OVERFLOW = -30,                 // if an operation with a float results in an overflow
        NOT_IOU_AMOUNT = -31,
        NOT_AN_AMOUNT = -32,
        CANT_RETURN_NEGATIVE = -33,
        NOT_AUTHORIZED = -34,
        PREVIOUS_FAILURE_PREVENTS_RETRY = -35,
        TOO_MANY_PARAMS = -36
    };

    enum ExitType : uint8_t {
        UNSET = 0,
        WASM_ERROR = 1,
        ROLLBACK = 2,
        ACCEPT = 3,
    };

    const int etxn_details_size = 105;
    const int max_slots = 255;
    const int max_nonce = 255;
    const int max_emit = 255;
    const int max_params = 16;
    const int drops_per_byte = 31250; //RH TODO make these  votable config option
    const double fee_base_multiplier = 1.1f;

    // RH TODO: consider replacing macros with templates, if SSVM compatible templates even exist

    #define LPAREN (
    #define RPAREN )
    #define EXPAND(...) __VA_ARGS__
    #define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
    #define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__
    #define SPLIT_1(D) EXPAND(CAT(SPLIT_, D))
    #define SPLIT_uint32_t
    #define SPLIT_int32_t
    #define SPLIT_uint64_t
    #define SPLIT_int64_t
    #define SPLIT_2(a, b) SPLIT_1(a), SPLIT_1(b)
    #define SPLIT_3(a, ...) SPLIT_1(a), SPLIT_2(__VA_ARGS__)
    #define SPLIT_4(a, ...) SPLIT_1(a), SPLIT_3(__VA_ARGS__)
    #define SPLIT_5(a, ...) SPLIT_1(a), SPLIT_4(__VA_ARGS__)
    #define SPLIT_6(a, ...) SPLIT_1(a), SPLIT_5(__VA_ARGS__)
    #define SPLIT_7(a, ...) SPLIT_1(a), SPLIT_6(__VA_ARGS__)
    #define SPLIT_8(a, ...) SPLIT_1(a), SPLIT_7(__VA_ARGS__)
    #define SPLIT_9(a, ...) SPLIT_1(a), SPLIT_8(__VA_ARGS__)
    #define SPLIT_10(a, ...) SPLIT_1(a), SPLIT_9(__VA_ARGS__)
    #define EMPTY()
    #define DEFER(id) id EMPTY()
    #define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
    #define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
    #define VA_NARGS(__drop, ...) VA_NARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
    #define STRIP_TYPES(...) DEFER(CAT(SPLIT_,VA_NARGS(NULL, __VA_ARGS__))CAT(LPAREN OBSTRUCT(__VA_ARGS__) RPAREN))

    // The above macros allow types to be stripped off a pramater list, used below for proxying call through class

    #define DECLARE_HOOK_FUNCTION(R, F, ...)\
        R F(hook::HookContext& hookCtx, SSVM::Runtime::Instance::MemoryInstance& memoryCtx, __VA_ARGS__);\
        class WasmFunction_##F : public SSVM::Runtime::HostFunction<WasmFunction_##F>\
        {\
            public:\
            hook::HookContext& hookCtx;\
            WasmFunction_##F(hook::HookContext& ctx) : hookCtx(ctx) {};\
            SSVM::Expect<R> body(SSVM::Runtime::Instance::MemoryInstance* memoryCtx, __VA_ARGS__)\
            {\
                R return_code = hook_api::F(hookCtx, *memoryCtx, STRIP_TYPES(__VA_ARGS__));\
                if (return_code == RC_ROLLBACK || return_code == RC_ACCEPT)\
                    return SSVM::Unexpect(SSVM::ErrCode::Terminated);\
                return return_code;\
            }\
        };

    #define DECLARE_HOOK_FUNCNARG(R, F)\
        R F(hook::HookContext& hookCtx, SSVM::Runtime::Instance::MemoryInstance& memoryCtx);\
        class WasmFunction_##F : public SSVM::Runtime::HostFunction<WasmFunction_##F>\
        {\
            public:\
            hook::HookContext& hookCtx;\
            WasmFunction_##F(hook::HookContext& ctx) : hookCtx(ctx) {};\
            SSVM::Expect<R> body(SSVM::Runtime::Instance::MemoryInstance* memoryCtx)\
            {\
                R return_code = hook_api::F(hookCtx, *memoryCtx);\
                if (return_code == RC_ROLLBACK || return_code == RC_ACCEPT)\
                    return SSVM::Unexpect(SSVM::ErrCode::Terminated);\
                return return_code;\
            }\
        };

    #define DEFINE_HOOK_FUNCTION(R, F, ...)\
        R hook_api::F(hook::HookContext& hookCtx, SSVM::Runtime::Instance::MemoryInstance& memoryCtx, __VA_ARGS__)

    #define DEFINE_HOOK_FUNCNARG(R, F)\
        R hook_api::F(hook::HookContext& hookCtx, SSVM::Runtime::Instance::MemoryInstance& memoryCtx)


    // RH NOTE: Find descriptions of api functions in ./impl/applyHook.cpp and hookapi.h (include for hooks)


    DECLARE_HOOK_FUNCTION(int32_t,  _g,                 uint32_t guard_id, uint32_t maxiter );

    DECLARE_HOOK_FUNCTION(int64_t,	accept,             uint32_t read_ptr,  uint32_t read_len, int64_t error_code );
    DECLARE_HOOK_FUNCTION(int64_t,	rollback,           uint32_t read_ptr,  uint32_t read_len, int64_t error_code );
    DECLARE_HOOK_FUNCTION(int64_t,	util_raddr,         uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len );
    DECLARE_HOOK_FUNCTION(int64_t,	util_accid,         uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len );
    DECLARE_HOOK_FUNCTION(int64_t,	util_verify,        uint32_t dread_ptr, uint32_t dread_len,
                                                        uint32_t sread_ptr, uint32_t sread_len,
                                                        uint32_t kread_ptr, uint32_t kread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	sto_validate,       uint32_t tread_ptr, uint32_t tread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	sto_subfield,       uint32_t read_ptr,  uint32_t read_len,  uint32_t field_id );
    DECLARE_HOOK_FUNCTION(int64_t,	sto_subarray,       uint32_t read_ptr,  uint32_t read_len,  uint32_t array_id );
    DECLARE_HOOK_FUNCTION(int64_t,	sto_emplace,        uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t sread_ptr, uint32_t sread_len,
                                                        uint32_t fread_ptr, uint32_t fread_len, uint32_t field_id );
    DECLARE_HOOK_FUNCTION(int64_t,  sto_erase,          uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len,  uint32_t field_id );

    DECLARE_HOOK_FUNCTION(int64_t,	util_sha512h,       uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len );
    DECLARE_HOOK_FUNCTION(int64_t,  util_keylet,        uint32_t write_ptr, uint32_t write_len, uint32_t keylet_type,
                                                        uint32_t a,         uint32_t b,         uint32_t c,
                                                        uint32_t d,         uint32_t e,         uint32_t f );
    DECLARE_HOOK_FUNCNARG(int64_t,	etxn_burden         );
    DECLARE_HOOK_FUNCTION(int64_t,	etxn_details,       uint32_t write_ptr, uint32_t write_len );
    DECLARE_HOOK_FUNCTION(int64_t,	etxn_fee_base,      uint32_t tx_byte_count);
    DECLARE_HOOK_FUNCTION(int64_t,	etxn_reserve,       uint32_t count );
    DECLARE_HOOK_FUNCNARG(int64_t,	etxn_generation     );
    DECLARE_HOOK_FUNCTION(int64_t,	emit,               uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len );

    DECLARE_HOOK_FUNCTION(int64_t,  float_set,          int32_t exponent,   int64_t mantissa );
    DECLARE_HOOK_FUNCTION(int64_t,  float_multiply,     int64_t float1,     int64_t float2 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_mulratio,     int64_t float1,     uint32_t round_up,
                                                        uint32_t numerator, uint32_t denominator );
    DECLARE_HOOK_FUNCTION(int64_t,  float_negate,       int64_t float1 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_compare,      int64_t float1,     int64_t float2, uint32_t mode );
    DECLARE_HOOK_FUNCTION(int64_t,  float_sum,          int64_t float1,     int64_t float2 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_sto,          uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t cread_ptr, uint32_t cread_len,
                                                        uint32_t iread_ptr, uint32_t iread_len,
                                                        int64_t float1,     uint32_t field_code);
    DECLARE_HOOK_FUNCTION(int64_t,  float_sto_set,      uint32_t read_ptr,  uint32_t read_len );
    DECLARE_HOOK_FUNCTION(int64_t,  float_invert,       int64_t float1 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_divide,       int64_t float1,     int64_t float2 );
    DECLARE_HOOK_FUNCNARG(int64_t,  float_one );

    DECLARE_HOOK_FUNCTION(int64_t,  float_exponent,     int64_t float1 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_exponent_set, int64_t float1,     int32_t exponent );
    DECLARE_HOOK_FUNCTION(int64_t,  float_mantissa,     int64_t float1 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_mantissa_set, int64_t float1,     int64_t mantissa );
    DECLARE_HOOK_FUNCTION(int64_t,  float_sign,         int64_t float1 );
    DECLARE_HOOK_FUNCTION(int64_t,  float_sign_set,     int64_t float1,     uint32_t negative );
    DECLARE_HOOK_FUNCTION(int64_t,  float_int,          int64_t float1,     uint32_t decimal_places, uint32_t abs );

    DECLARE_HOOK_FUNCTION(int64_t,	hook_account,       uint32_t write_ptr, uint32_t write_len );
    DECLARE_HOOK_FUNCTION(int64_t,	hook_hash,          uint32_t write_ptr, uint32_t write_len, int32_t hook_no );
    DECLARE_HOOK_FUNCNARG(int64_t,	fee_base            );
    DECLARE_HOOK_FUNCNARG(int64_t,	ledger_seq          );
    DECLARE_HOOK_FUNCTION(int64_t,  ledger_last_hash,   uint32_t write_ptr, uint32_t write_len );
    DECLARE_HOOK_FUNCTION(int64_t,	nonce,              uint32_t write_ptr, uint32_t write_len );


    DECLARE_HOOK_FUNCTION(int64_t,  hook_param_set,     uint32_t read_ptr,  uint32_t read_len,
                                                        uint32_t kread_ptr, uint32_t kread_len,
                                                        uint32_t hread_ptr, uint32_t hread_len);
    
    DECLARE_HOOK_FUNCTION(int64_t,  hook_param,         uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t read_ptr,  uint32_t read_len);

    DECLARE_HOOK_FUNCTION(int64_t,  hook_skip,          uint32_t read_ptr,  uint32_t read_len, uint32_t flags);
    DECLARE_HOOK_FUNCNARG(int64_t,  hook_pos);

    DECLARE_HOOK_FUNCTION(int64_t,	slot,               uint32_t write_ptr, uint32_t write_len, uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_clear,         uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_count,         uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_id,            uint32_t write_ptr, uint32_t write_len, uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_set,           uint32_t read_ptr,  uint32_t read_len, int32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_size,          uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_subarray,      uint32_t parent_slot, uint32_t array_id, uint32_t new_slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_subfield,      uint32_t parent_slot, uint32_t field_id, uint32_t new_slot );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_type,          uint32_t slot_no, uint32_t flags );
    DECLARE_HOOK_FUNCTION(int64_t,	slot_float,         uint32_t slot_no );

    DECLARE_HOOK_FUNCTION(int64_t,	state_set,          uint32_t read_ptr,  uint32_t read_len,
                                                        uint32_t kread_ptr, uint32_t kread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	state_foreign_set,  uint32_t read_ptr,  uint32_t read_len,
                                                        uint32_t kread_ptr, uint32_t kread_len,
                                                        uint32_t nread_ptr, uint32_t nread_len,
                                                        uint32_t aread_ptr, uint32_t aread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	state,              uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t kread_ptr, uint32_t kread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	state_foreign,      uint32_t write_ptr, uint32_t write_len,
                                                        uint32_t kread_ptr, uint32_t kread_len,
                                                        uint32_t nread_ptr, uint32_t nread_len,
                                                        uint32_t aread_ptr, uint32_t aread_len );
    DECLARE_HOOK_FUNCTION(int64_t,	trace_slot,         uint32_t read_ptr, uint32_t read_len, uint32_t slot );
    DECLARE_HOOK_FUNCTION(int64_t,	trace,              uint32_t mread_ptr, uint32_t mread_len,
                                                        uint32_t dread_ptr, uint32_t dread_len, uint32_t as_hex );
    DECLARE_HOOK_FUNCTION(int64_t,	trace_num,          uint32_t read_ptr, uint32_t read_len, int64_t number );
    DECLARE_HOOK_FUNCTION(int64_t,	trace_float,        uint32_t read_ptr, uint32_t read_len, int64_t  float1 );

    DECLARE_HOOK_FUNCNARG(int64_t,	otxn_burden         );
    DECLARE_HOOK_FUNCTION(int64_t,	otxn_field,         uint32_t write_ptr, uint32_t write_len, uint32_t field_id );
    DECLARE_HOOK_FUNCTION(int64_t,	otxn_field_txt,     uint32_t write_ptr, uint32_t write_len, uint32_t field_id );
    DECLARE_HOOK_FUNCNARG(int64_t,	otxn_generation     );
    DECLARE_HOOK_FUNCTION(int64_t,	otxn_id,            uint32_t write_ptr, uint32_t write_len, uint32_t flags );
    DECLARE_HOOK_FUNCNARG(int64_t,	otxn_type           );
    DECLARE_HOOK_FUNCTION(int64_t,	otxn_slot,          uint32_t slot_no );


} /* end namespace hook_api */

namespace hook {

    bool canHook(ripple::TxType txType, uint64_t hookOn);

    struct HookResult;

    HookResult apply(
            ripple::uint256 const&,
            ripple::uint256 const&,
            ripple::uint256 const&,
            ripple::Blob const&,
            std::map<
                std::vector<uint8_t>,          /* param name  */
                std::vector<uint8_t>           /* param value */
            > const& parameters,
            std::map<
                ripple::uint256,
                std::map<
                    std::vector<uint8_t>,          /* param name  */
                    std::vector<uint8_t>           /* param value */
                >> const& parameterOverrides,
            ripple::ApplyContext&,
            ripple::AccountID const&,
            bool callback,
            uint32_t wasmParam = 0,
            int32_t hookChainPosition = -1);

    struct HookContext;

    uint32_t maxHookStateDataSize(void);
    uint32_t maxHookWasmSize(void);
    uint32_t maxHookParameterSize(void);
    uint32_t maxHookChainLength(void);

    uint32_t computeExecutionFee(uint64_t instructionCount);
    uint32_t computeCreationFee(uint64_t byteCount);

    struct HookResult
    {
        ripple::uint256     const&      hookSetTxnID;
        ripple::uint256     const&      hookHash;
        ripple::Keylet      const&      accountKeylet;
        ripple::Keylet      const&      ownerDirKeylet;
        ripple::Keylet      const&      hookKeylet;
        ripple::AccountID   const&      account;
        ripple::AccountID   const&      otxnAccount;
        ripple::uint256     const&      hookNamespace;

        std::queue<std::shared_ptr<ripple::Transaction>> emittedTxn {}; // etx stored here until accept/rollback
        std::shared_ptr<
                std::map<ripple::AccountID,     // account to whom the state belongs
                std::map<ripple::uint256,       // namespace
                std::map<ripple::uint256,       // state key
                std::pair<
                    bool,                       // has been modified
                    ripple::Blob>>>>>           // actual state data
                        changedState;
        std::map<
            ripple::uint256,                    // hook hash
            std::map<
                std::vector<uint8_t>,                // hook param name
                std::vector<uint8_t>                 // hook param value
            >> hookParamOverrides;

        std::map<
            std::vector<uint8_t>,
            std::vector<uint8_t>>
                const& hookParams;
        std::set<ripple::uint256> hookSkips;
        hook_api::ExitType exitType = hook_api::ExitType::ROLLBACK;
        std::string exitReason {""};
        int64_t exitCode {-1};
        uint64_t instructionCount {0};
        bool callback = false;
        uint32_t wasmParam = 0;
        uint32_t overrideCount = 0;
        int32_t hookChainPosition = -1;
        bool foreignStateSetDisabled = false;
    };

    class HookModule;

    struct SlotEntry
    {
        std::vector<uint8_t> id;
        std::shared_ptr<const ripple::STObject> storage;
        const ripple::STBase* entry; // raw pointer into the storage, that can be freely pointed around inside
    };

    struct HookContext {
        ripple::ApplyContext& applyCtx;
        // slots are used up by requesting objects from inside the hook
        // the map stores pairs consisting of a memory view and whatever shared or unique ptr is required to
        // keep the underlying object alive for the duration of the hook's execution
        // slot number -> { keylet or hash, { pointer to current object, storage for that object } }
        std::map<int, SlotEntry> slot {};
        int slot_counter { 1 };
        std::queue<int> slot_free {};
        int64_t expected_etxn_count { -1 }; // make this a 64bit int so the uint32 from the hookapi cant overflow it
        int nonce_counter { 0 }; // incremented whenever nonce is called to ensure unique nonces
        std::map<ripple::uint256, bool> nonce_used {};
        uint32_t generation = 0; // used for caching, only generated when txn_generation is called
        int64_t burden = 0;      // used for caching, only generated when txn_burden is called
        int64_t fee_base = 0;
        std::map<uint32_t, uint32_t> guard_map {}; // iteration guard map <id -> upto_iteration>
        HookResult result;
        std::optional<ripple::STObject> emitFailure;    // if this is a callback from a failed
                                                        // emitted txn then this optional becomes
                                                        // populated with the SLE
        const HookModule* module = 0;
    };


    ripple::TER
    setHookState(
        HookResult& hookResult,
        ripple::ApplyContext& applyCtx,
        ripple::AccountID const& acc,
        ripple::uint256 const& ns,
        ripple::uint256 const & key,
        ripple::Slice const& data);

    // commit changes to ledger flags
    enum cclFlags : uint8_t {
        cclREMOVE = 0b10U,
        cclAPPLY  = 0b01U
    };

    // finalize the changes the hook made to the ledger
    void commitChangesToLedger( hook::HookResult& hookResult, ripple::ApplyContext&, uint8_t );

    #define ADD_HOOK_FUNCTION(F, ctx)\
        addHostFunc(#F, std::make_unique<hook_api::WasmFunction_##F>(ctx))

    class HookModule : public SSVM::Runtime::ImportObject
    {
   //RH TODO UPTO put the hook-api functions here!
   //then wrap/proxy them with the appropriate DECLARE_HOOK classes
   //and add those below. HookModule::otxn_id() ...
    public:
        HookContext hookCtx;

        HookModule(HookContext& ctx) : SSVM::Runtime::ImportObject("env"), hookCtx(ctx)
        {
            ctx.module = this;

            ADD_HOOK_FUNCTION(_g, ctx);
            ADD_HOOK_FUNCTION(accept, ctx);
            ADD_HOOK_FUNCTION(rollback, ctx);
            ADD_HOOK_FUNCTION(util_raddr, ctx);
            ADD_HOOK_FUNCTION(util_accid, ctx);
            ADD_HOOK_FUNCTION(util_verify, ctx);
            ADD_HOOK_FUNCTION(util_sha512h, ctx);
            ADD_HOOK_FUNCTION(sto_validate, ctx);
            ADD_HOOK_FUNCTION(sto_subfield, ctx);
            ADD_HOOK_FUNCTION(sto_subarray, ctx);
            ADD_HOOK_FUNCTION(sto_emplace, ctx);
            ADD_HOOK_FUNCTION(sto_erase, ctx);
            ADD_HOOK_FUNCTION(util_keylet, ctx);

            ADD_HOOK_FUNCTION(emit, ctx);
            ADD_HOOK_FUNCTION(etxn_burden, ctx);
            ADD_HOOK_FUNCTION(etxn_fee_base, ctx);
            ADD_HOOK_FUNCTION(etxn_details, ctx);
            ADD_HOOK_FUNCTION(etxn_reserve, ctx);
            ADD_HOOK_FUNCTION(etxn_generation, ctx);

            ADD_HOOK_FUNCTION(float_set, ctx);
            ADD_HOOK_FUNCTION(float_multiply, ctx);
            ADD_HOOK_FUNCTION(float_mulratio, ctx);
            ADD_HOOK_FUNCTION(float_negate, ctx);
            ADD_HOOK_FUNCTION(float_compare, ctx);
            ADD_HOOK_FUNCTION(float_sum, ctx);
            ADD_HOOK_FUNCTION(float_sto, ctx);
            ADD_HOOK_FUNCTION(float_sto_set, ctx);
            ADD_HOOK_FUNCTION(float_invert, ctx);
            ADD_HOOK_FUNCTION(float_mantissa, ctx);
            ADD_HOOK_FUNCTION(float_exponent, ctx);

            ADD_HOOK_FUNCTION(float_divide, ctx);
            ADD_HOOK_FUNCTION(float_one, ctx);
            ADD_HOOK_FUNCTION(float_mantissa, ctx);
            ADD_HOOK_FUNCTION(float_mantissa_set, ctx);
            ADD_HOOK_FUNCTION(float_exponent, ctx);
            ADD_HOOK_FUNCTION(float_exponent_set, ctx);
            ADD_HOOK_FUNCTION(float_sign, ctx);
            ADD_HOOK_FUNCTION(float_sign_set, ctx);
            ADD_HOOK_FUNCTION(float_int, ctx);



            ADD_HOOK_FUNCTION(otxn_burden, ctx);
            ADD_HOOK_FUNCTION(otxn_generation, ctx);
            ADD_HOOK_FUNCTION(otxn_field_txt, ctx);
            ADD_HOOK_FUNCTION(otxn_field, ctx);
            ADD_HOOK_FUNCTION(otxn_id, ctx);
            ADD_HOOK_FUNCTION(otxn_type, ctx);
            ADD_HOOK_FUNCTION(otxn_slot, ctx);
            ADD_HOOK_FUNCTION(hook_account, ctx);
            ADD_HOOK_FUNCTION(hook_hash, ctx);
            ADD_HOOK_FUNCTION(fee_base, ctx);
            ADD_HOOK_FUNCTION(ledger_seq, ctx);
            ADD_HOOK_FUNCTION(ledger_last_hash, ctx);
            ADD_HOOK_FUNCTION(nonce, ctx);

            ADD_HOOK_FUNCTION(hook_param, ctx);
            ADD_HOOK_FUNCTION(hook_param_set, ctx);
            ADD_HOOK_FUNCTION(hook_skip, ctx);
            ADD_HOOK_FUNCTION(hook_pos, ctx);

            ADD_HOOK_FUNCTION(state, ctx);
            ADD_HOOK_FUNCTION(state_foreign, ctx);
            ADD_HOOK_FUNCTION(state_set, ctx);
            ADD_HOOK_FUNCTION(state_foreign_set, ctx);

            ADD_HOOK_FUNCTION(slot, ctx);
            ADD_HOOK_FUNCTION(slot_clear, ctx);
            ADD_HOOK_FUNCTION(slot_count, ctx);
            ADD_HOOK_FUNCTION(slot_id, ctx);
            ADD_HOOK_FUNCTION(slot_set, ctx);
            ADD_HOOK_FUNCTION(slot_size, ctx);
            ADD_HOOK_FUNCTION(slot_subarray, ctx);
            ADD_HOOK_FUNCTION(slot_subfield, ctx);
            ADD_HOOK_FUNCTION(slot_type, ctx);
            ADD_HOOK_FUNCTION(slot_float, ctx);

            ADD_HOOK_FUNCTION(trace, ctx);
            ADD_HOOK_FUNCTION(trace_slot, ctx);
            ADD_HOOK_FUNCTION(trace_num, ctx);
            ADD_HOOK_FUNCTION(trace_float, ctx);

            SSVM::AST::Limit TabLimit(10, 20);
            addHostTable("table", std::make_unique<SSVM::Runtime::Instance::TableInstance>(
                    SSVM::ElemType::FuncRef, TabLimit));
            SSVM::AST::Limit MemLimit(1, 1);
            addHostMemory("memory", std::make_unique<SSVM::Runtime::Instance::MemoryInstance>(MemLimit));
        }
        virtual ~HookModule() = default;
    };

}

#endif
