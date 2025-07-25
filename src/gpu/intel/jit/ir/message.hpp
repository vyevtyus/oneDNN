/*******************************************************************************
* Copyright 2022-2025 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef GPU_INTEL_JIT_IR_MESSAGE_HPP
#define GPU_INTEL_JIT_IR_MESSAGE_HPP

#include "gpu/intel/jit/ir/fma.hpp"
#include "gpu/intel/jit/ir/gemm_schedule.hpp"
#include "gpu/intel/jit/ir/ir.hpp"
#include "gpu/intel/jit/ir/tensor.hpp"

namespace dnnl {
namespace impl {
namespace gpu {
namespace intel {
namespace jit {

enum class send_kind_t {
    undef,
    _2d,
    block,
    scattered,
};

static auto send_kind_names = nstl::to_array({
        make_enum_name(send_kind_t::undef, "undef"),
        make_enum_name(send_kind_t::_2d, "2d"),
        make_enum_name(send_kind_t::block, "block"),
        make_enum_name(send_kind_t::scattered, "scattered"),
});
GPU_DEFINE_PARSE_ENUM(send_kind_t, send_kind_names)

// Send operation kind.
enum class send_op_t {
    undef,
    atomic_add,
    atomic_fadd,
    atomic_cmpwr,
    load,
    load_2d,
    prefetch,
    prefetch_2d,
    store,
    store_2d,
};

static auto send_op_names = nstl::to_array({
        make_enum_name(send_op_t::undef, "undef"),
        make_enum_name(send_op_t::atomic_add, "atomic_add"),
        make_enum_name(send_op_t::atomic_fadd, "atomic_fadd"),
        make_enum_name(send_op_t::atomic_cmpwr, "atomic_cmpwr"),
        make_enum_name(send_op_t::load, "load"),
        make_enum_name(send_op_t::load_2d, "load_2d"),
        make_enum_name(send_op_t::prefetch, "prefetch"),
        make_enum_name(send_op_t::prefetch_2d, "prefetch_2d"),
        make_enum_name(send_op_t::store, "store"),
        make_enum_name(send_op_t::store_2d, "store_2d"),
});
GPU_DEFINE_PARSE_ENUM(send_op_t, send_op_names)

// Send address model.
enum class send_address_t {
    a64,
    slm,
};

enum class send_cache_hint_t {
    undef,
    load_once,
};

static auto send_cache_hint_names = nstl::to_array({
        make_enum_name(send_cache_hint_t::undef, "cache:undef"),
        make_enum_name(send_cache_hint_t::load_once, "cache:load_once"),
});

GPU_DEFINE_PARSE_ENUM(send_cache_hint_t, send_cache_hint_names)

struct block_2d_info_t {
    bool is_empty() const { return surface_width == 0; }

    bool operator==(const block_2d_info_t &other) const {
        if (is_empty() != other.is_empty()) return false;
        if (is_empty()) return true;
        return (surface_width == other.surface_width)
                && (surface_height == other.surface_height)
                && (surface_pitch == other.surface_pitch)
                && (width == other.width) && (height == other.height)
                && (count == other.count) && (vnni == other.vnni)
                && (transpose == other.transpose);
    }

    std::string str() const {
        std::ostringstream oss;
        oss << count << "x";
        oss << height << "x";
        oss << width;
        if (vnni || transpose) {
            oss << ".";
            if (vnni) oss << "v";
            if (transpose) oss << "t";
        }
        return oss.str();
    }

    // Encoded in header.
    int surface_width = 0;
    int surface_height = 0;
    int surface_pitch = 0;
    int width = 0;
    int height = 0;
    int count = 0;
    // Part of descriptor.
    bool vnni = false;
    bool transpose = false;
};

// Function representing send messages.
class send_t : public func_impl_t {
public:
    IR_DECL_TYPE(send_t)

    static func_t make(const hw_t &hw, send_op_t op, send_address_t address,
            const type_t &type, int slots, bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        return make(hw, op, address, type, slots, default_slot_mask,
                hw >= ngen::HW::XeHPC, zero_out, cache_hint);
    }

    static func_t make(const hw_t &hw, send_op_t op, send_address_t address,
            const type_t &type, int slots, bool is_lsc, bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        return make(hw, op, address, type, slots, default_slot_mask, is_lsc,
                zero_out, cache_hint);
    }

    static func_t make(const hw_t &hw, send_op_t op, send_address_t address,
            const type_t &type, int slots, uint32_t slot_mask, bool is_lsc,
            bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        return func_t(new send_t(hw, op, address, type, slots, slot_mask,
                is_lsc, zero_out, cache_hint));
    }

    static func_t make(const hw_t &hw, send_op_t op, send_address_t address,
            const type_t &type, int slots, uint32_t slot_mask, bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        return make(hw, op, address, type, slots, slot_mask,
                hw >= ngen::HW::XeHPC, zero_out, cache_hint);
    }

    static func_t make_2d(const hw_t &hw, send_op_t op, const type_t &type,
            int surface_width, int surface_height, int surface_pitch, int width,
            int height, int count, bool vnni, bool transpose, bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        block_2d_info_t info;
        info.surface_width = surface_width;
        info.surface_height = surface_height;
        info.surface_pitch = surface_pitch;
        info.width = width;
        info.height = height;
        info.count = count;
        info.vnni = vnni;
        info.transpose = transpose;
        return func_t(new send_t(hw, op, type, zero_out, info, cache_hint));
    }

    static func_t make_2d(const hw_t &hw, send_op_t op, const type_t &type,
            int width, int height, int count, bool vnni, bool transpose,
            bool zero_out,
            send_cache_hint_t cache_hint = send_cache_hint_t::undef) {
        return make_2d(hw, op, type, /*surface_width=*/0, /*surface_height=*/0,
                /*surface_pitch=*/0, width, height, count, vnni, transpose,
                zero_out, cache_hint);
    }

    bool is_equal(const object_impl_t &obj) const override {
        if (!obj.is<self_type>()) return false;
        auto &other = obj.as<self_type>();

        // hw is not compared as cross-hardware IR operations are not expected.
        return (op == other.op) && (address == other.address)
                && (type == other.type) && (slots == other.slots)
                && (slot_mask == other.slot_mask) && (is_lsc == other.is_lsc)
                && (fill_buf == other.fill_buf)
                && (block_2d_info == other.block_2d_info)
                && (cache_hint == other.cache_hint);
    }
    std::string str() const override {
        std::ostringstream oss;
        oss << to_string(op);
        oss << ".";
        oss << type.str();
        if (is_scattered()) oss << "x" << slots;
        if (is_2d()) oss << "." << block_2d_info.str();
        if (!fill_buf) oss << ".nofill";
        if (cache_hint != send_cache_hint_t::undef)
            oss << "." << to_string(cache_hint);
        return oss.str();
    }

    IR_DEFINE_ARG_GET(mem_buf, 0)
    IR_DEFINE_ARG_GET(mem_off, 1)
    IR_DEFINE_ARG_GET(header_buf, 1)
    IR_DEFINE_ARG_GET(reg_buf, 2)
    IR_DEFINE_ARG_GET(mask, 3)
    IR_DEFINE_ARG_GET(x, 4)
    IR_DEFINE_ARG_GET(y, 5)
    IR_DEFINE_ARG_GET(fill_pattern, 6)

    // Header offsets in bytes for 2D block messages.
    static int header_2d_off_base() { return 0; }
    static int header_2d_off_surface_width() { return 8; }
    static int header_2d_off_surface_height() { return 12; }
    static int header_2d_off_surface_pitch() { return 16; }
    static int header_2d_off_x() { return 20; }
    static int header_2d_off_y() { return 24; }
    static int header_2d_off_whc() { return 28; }

    stmt_t operator()(const expr_t &mem_buf, const expr_t &mem_off,
            const expr_t &reg_buf, const expr_t &mask,
            const expr_t &x = expr_t(), const expr_t &y = expr_t(),
            const expr_t &pattern = expr_t()) const {
        return call({mem_buf, mem_off, reg_buf, mask, x, y, pattern});
    }

    bool is_atomic() const {
        return utils::one_of(op, send_op_t::atomic_add, send_op_t::atomic_fadd,
                send_op_t::atomic_cmpwr);
    }
    bool is_load() const { return op == send_op_t::load; }
    bool is_load_2d() const { return op == send_op_t::load_2d; }
    bool is_prefetch() const { return op == send_op_t::prefetch; }
    bool is_prefetch_2d() const { return op == send_op_t::prefetch_2d; }
    bool is_store() const { return op == send_op_t::store; }
    bool is_store_2d() const { return op == send_op_t::store_2d; }
    bool is_2d() const {
        return is_load_2d() || is_store_2d() || is_prefetch_2d();
    }
    bool is_a64() const { return address == send_address_t::a64; }
    bool is_slm() const { return address == send_address_t::slm; }

    bool is_block() const {
        return utils::one_of(
                type.kind(), type_kind_t::oword, type_kind_t::hword);
    }

    bool is_scattered() const { return !is_block() && !is_2d(); }

    // Size of memory (global memory or SLM) to access.
    int access_size() const {
        if (is_2d()) {
            auto &info = block_2d_info;
            return type.size() * info.width * info.height * info.count;
        }
        return type.size() * slots;
    }

    int payload_type_stride() const {
        gpu_assert(!is_2d());
        return std::max(4, type.size());
    }

    // Full size of payload GRF buffer for this message. Buffer may be strided
    // and/or require GRF boundary round-up.
    int payload_size() const {
        if (is_2d()) {
            auto &info = block_2d_info;
            int w = info.width;
            int h = info.height;
            int c = info.count;
            if (info.transpose) {
                h = utils::rnd_up_pow2(h);
            } else {
                w = utils::rnd_up_pow2(w);
            }
            return utils::rnd_up(type.size() * w * h, grf_size()) * c;
        }
        int sz = payload_type_stride() * slots;
        return utils::rnd_up(sz, grf_size());
    }

    int alignment() const {
        if (is_2d()) return 128;
        if (is_block()) return type.scalar().size();
        return 1;
    }

    int mask_size() const {
        if (is_2d()) return access_size();
        if (is_block()) {
            // LSC messages use SIMT1 execution mask (one mask per message).
            if (is_lsc) return type.size();
            return 4;
        }

        if (is_scattered()) return type.size();

        gpu_error_not_expected();
        return 0;
    }

    int nmasks() const {
        if (is_2d()) return 1;
        int masks = ir_utils::safe_divide(type.size() * slots, mask_size());
        if (hw < ngen::HW::XeHPC && is_block() && masks > 16) {
            // Round-robin masking, 16 bits are reused with dword granularity.
            gpu_assert(masks % 16 == 0);
            masks = 16;
        }
        return masks;
    }

    int address_size() const { return is_a64() ? 8 : 4; }

    type_t address_type(bool is_signed = false, int elems = 1) const {
        int bits = address_size() * 8;
        return is_signed ? type_t::s(bits, elems) : type_t::u(bits, elems);
    }

    // Size of header in bytes.
    int header_size() const {
        if (is_2d()) return grf_size();
        return utils::rnd_up(address_size() * slots, grf_size());
    }

    // Generates a statement to store (and maybe convert) the offset to the
    // message header according to the message description.
    stmt_t create_offset_store(const expr_t &header_buf, const expr_t &mem_buf,
            const expr_t &mem_off, bool is_signed_offset = false) const;

    bool is_supported() const;

    bool has_default_slot_mask() const {
        uint32_t all_slots_mask = (slots == 32 ? 0xFFFFFFFF : (1 << slots) - 1);
        return (slot_mask & all_slots_mask) == all_slots_mask;
    }

    static std::vector<func_t> get_all(const hw_t &hw, send_op_t op,
            send_address_t address, const type_t &mem_type, bool zero_out,
            send_cache_hint_t cache_hint);

    hw_t hw;
    send_op_t op;
    send_address_t address;
    type_t type;
    int slots;
    uint32_t slot_mask;
    bool is_lsc;
    bool fill_buf;

    block_2d_info_t block_2d_info;
    send_cache_hint_t cache_hint;

    static const uint32_t default_slot_mask = 0xFFFFFFFF;

private:
    int grf_size() const { return hw.grf_size(); }

    bool is_xe_hp_plus() const { return hw >= ngen::HW::XeHP; }

    bool is_xe_hpc_plus() const { return hw >= ngen::HW::XeHPC; }

    send_t(const hw_t &hw, send_op_t op, send_address_t address,
            const type_t &type, int slots, uint32_t slot_mask, bool is_lsc,
            bool zero_out, send_cache_hint_t cache_hint)
        : func_impl_t(_type_info())
        , hw(hw)
        , op(op)
        , address(address)
        , type(type)
        , slots(slots)
        , slot_mask(slot_mask)
        , is_lsc(is_lsc)
        , fill_buf(zero_out)
        , cache_hint(cache_hint) {}

    send_t(const hw_t &hw, send_op_t op, const type_t &type, bool zero_out,
            const block_2d_info_t &block_2d_info, send_cache_hint_t cache_hint)
        : func_impl_t(_type_info())
        , hw(hw)
        , op(op)
        , address(send_address_t::a64)
        , type(type)
        , slots(1)
        , slot_mask(default_slot_mask)
        , is_lsc(true)
        , fill_buf(zero_out)
        , block_2d_info(block_2d_info)
        , cache_hint(cache_hint) {
        gpu_assert(utils::one_of(op, send_op_t::load_2d, send_op_t::store_2d,
                send_op_t::prefetch_2d));
        if (is_store_2d()) {
            gpu_assert(!block_2d_info.vnni);
            gpu_assert(!block_2d_info.transpose);
        }
    }
};

ngen::CacheSettingsLSC get_cache_settings(const send_t &send, const hw_t &hw);

class memory_walker_t;
class layout_walker_t;

struct send_2d_hint_t {
    type_t type;
    bool enable = false;
    bool vnni = false;
    bool transpose = false;
    int vnni_permute_factor = 0;
    int width = 0;
    int height = 0;
};

struct send_params_t {
    send_params_t() = default;
    send_params_t(const hw_t &hw, const type_t &mem_type, send_op_t send_op)
        : hw(hw), mem_type(mem_type), send_op(send_op), use_send_plan(true) {}

    send_op_t convert(const send_op_t &op) const {
        if (hint_2d.enable) {
            if (op == send_op_t::load) return send_op_t::load_2d;
            if (op == send_op_t::store) return send_op_t::store_2d;
            if (op == send_op_t::prefetch) return send_op_t::prefetch_2d;
        }
        return op;
    }

    bool is_slm() const { return send_address == send_address_t::slm; }
    bool is_prefetch() const {
        return utils::one_of(
                send_op, send_op_t::prefetch, send_op_t::prefetch_2d);
    }

    hw_t hw;
    type_t mem_type;
    send_op_t send_op;
    send_address_t send_address;
    send_cache_hint_t cache_hint;
    send_2d_hint_t hint_2d;
    bool prefer_dense = false;
    bool use_send_plan = false;
    bool try_legacy = true;
};

// Generates loads or stores to move data between memory (global or SLM) and
// GRF. Memory view is a parameter. GRF payload layout is deduced
// automatically, according to the decomposition into messages.
class access_builder_t {
public:
    access_builder_t(ir_context_t &ir_ctx, const view_t &mem_view,
            const expr_t &mem_buf, const expr_t &reg_buf, send_op_t send_op,
            send_address_t send_address, send_cache_hint_t send_cache_hint,
            send_params_t &send_params, bool zero_out);
    access_builder_t(access_builder_t &&);
    ~access_builder_t();

    const layout_t &reg_layout() const { return reg_layout_; }
    int reg_buf_size() const {
        if (reg_buf_size_ == 0)
            return into<int>(utils::rnd_up(reg_layout_.size(), grf_size()));
        return reg_buf_size_;
    }
    const stmt_t &stmt() const { return stmt_; }

    std::string str() const {
        std::ostringstream oss;
        oss << "Memory view:          " << mem_view_ << std::endl;
        oss << "Register layout:      " << reg_layout_ << std::endl;
        oss << "Register buffer:      " << reg_buf_ << std::endl;
        oss << "Register buffer size: " << reg_buf_size() << " ("
            << reg_buf_size() / grf_size() << " regs)" << std::endl;
        oss << "Statement:            " << std::endl << stmt_;
        return oss.str();
    }

private:
    void build();
    bool try_build(const layout_t &try_layout, memory_walker_t &mem_walker);
    bool try_build_2d(send_params_t &send_params);
    bool fixup_send_2d_params(const type_t &send_type, bool vnni,
            bool transpose, bool use_xy, int &W, int &H, int &P, int &w, int &h,
            int &c, int &vnni_permute_factor);

    bool check_2d_mask(const tile_t &tile, const coord_t &coord,
            bool use_virtual_surface, dim_idx_t w_idx, dim_idx_t h_idx,
            expr_t &mask) const;

    std::vector<layout_t> candidate_payload_layouts() const;
    stmt_t create_send_stmt(
            const send_t &send, const memory_walker_t &memory_walker);
    int grf_size() const { return ir_ctx_->hw().grf_size(); }

    ir_context_t *ir_ctx_ = nullptr;
    view_t mem_view_;
    expr_t mem_buf_;
    expr_t reg_buf_;
    send_op_t send_op_;
    send_address_t send_address_;
    send_cache_hint_t send_cache_hint_;

    type_t mem_type_;

    std::unique_ptr<layout_walker_t> reg_layout_walker_;

    layout_t reg_layout_;
    int reg_buf_size_ = 0;
    bool zero_out_ = true;
    stmt_t stmt_;
};

send_params_t get_send_params(const exec_config_t &exec_cfg, send_op_t send_op,
        send_address_t send_address, const view_t &view,
        send_cache_hint_t cache_hint = send_cache_hint_t::undef,
        fma_kind_t fma_kind = fma_kind_t::undef,
        abc_kind_t abc_kind = abc_kind_t::undef, bool allow_2d = false);

send_params_t get_send_params(const exec_config_t &exec_cfg, send_op_t send_op,
        send_address_t send_address, fma_kind_t fma_kind, abc_kind_t abc_kind,
        const view_t &view, const gemm_schedule_t &gemm_schedule,
        bool allow_2d = true);

inline send_params_t get_send_params(const exec_config_t &exec_cfg,
        send_op_t send_op, send_address_t send_address, const view_t &view,
        bool allow_2d) {
    return get_send_params(exec_cfg, send_op, send_address, view,
            send_cache_hint_t::undef, fma_kind_t::undef, abc_kind_t::undef,
            allow_2d);
}

inline access_builder_t make_access_builder(ir_context_t &ir_ctx,
        const view_t &mem_view, const expr_t &mem_buf, const expr_t &reg_buf,
        send_params_t &send_params, bool zero_out = true) {
    return access_builder_t(ir_ctx, mem_view, mem_buf, reg_buf,
            send_params.send_op, send_params.send_address,
            send_params.cache_hint, send_params, zero_out);
}

inline access_builder_t make_access_builder(ir_context_t &ir_ctx,
        const view_t &mem_view, const expr_t &mem_buf, const expr_t &reg_buf,
        send_op_t send_op, send_address_t send_address,
        send_cache_hint_t cache_hint = send_cache_hint_t::undef,
        bool zero_out = true) {
    auto send_params = get_send_params(
            ir_ctx.exec_cfg(), send_op, send_address, mem_view, cache_hint);
    return make_access_builder(
            ir_ctx, mem_view, mem_buf, reg_buf, send_params, zero_out);
}

} // namespace jit
} // namespace intel
} // namespace gpu
} // namespace impl
} // namespace dnnl

#endif
