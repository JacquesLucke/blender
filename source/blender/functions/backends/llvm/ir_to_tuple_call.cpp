#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace FN {

	typedef std::function<void (
		CodeBuilder &builder,
		CodeInterface &interface,
		const BuildIRSettings &settings)> BuildIRFunction;

	static llvm::Value *lookup_tuple_address(
		CodeBuilder &builder,
		llvm::Value *data_addr,
		llvm::Value *offsets_addr,
		uint index)
	{
		llvm::Value *offset_addr = builder.CreateConstGEP1_32(offsets_addr, index);
		llvm::Value *offset = builder.CreateLoad(offset_addr);
		llvm::Value *value_byte_addr = builder.CreateGEP(data_addr, offset);
		return value_byte_addr;
	}

	static llvm::Function *insert_tuple_call_function(
		SharedFunction &fn,
		BuildIRFunction build_ir,
		llvm::Module *module)
	{
		llvm::LLVMContext &context = module->getContext();

		llvm::Type *void_ty = llvm::Type::getVoidTy(context);
		llvm::Type *byte_ptr_ty = llvm::Type::getInt8PtrTy(context);
		llvm::Type *int_ptr_ty = llvm::Type::getInt32PtrTy(context);

		LLVMTypes input_types = {
			byte_ptr_ty,
			int_ptr_ty,
			byte_ptr_ty,
			int_ptr_ty,
		};

		llvm::FunctionType *function_type = llvm::FunctionType::get(
			void_ty, to_array_ref(input_types), false);

		llvm::Function *function = llvm::Function::Create(
			function_type,
			llvm::GlobalValue::LinkageTypes::ExternalLinkage,
			fn->name(),
			module);


		llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
		CodeBuilder builder(bb);

		llvm::Value *fn_in_data = function->arg_begin() + 0;
		llvm::Value *fn_in_offsets = function->arg_begin() + 1;
		llvm::Value *fn_out_data = function->arg_begin() + 2;
		llvm::Value *fn_out_offsets = function->arg_begin() + 3;

		LLVMValues input_values;
		for (uint i = 0; i < fn->signature().inputs().size(); i++) {
			llvm::Value *value_byte_addr = lookup_tuple_address(
				builder, fn_in_data, fn_in_offsets, i);

			LLVMTypeInfo *type_info = get_type_info(
				fn->signature().inputs()[i].type());
			llvm::Value *value = type_info->build_load_ir__relocate(
				builder, value_byte_addr);

			input_values.append(value);
		}

		LLVMValues output_values(fn->signature().outputs().size());
		BuildIRSettings settings;
		CodeInterface interface(input_values, output_values);
		build_ir(builder, interface, settings);

		for (uint i = 0; i < output_values.size(); i++) {
			llvm::Value *value_byte_addr = lookup_tuple_address(
				builder, fn_out_data, fn_out_offsets, i);

			LLVMTypeInfo *type_info = get_type_info(
				fn->signature().outputs()[i].type());
			type_info->build_store_ir__relocate(
				builder, output_values[i], value_byte_addr);
		}

		builder.CreateRetVoid();

		return function;
	}

	typedef void (*LLVMCallFN)(
		void *data_in,
		const uint *offsets_in,
		void *data_out,
		const uint *offsets_out);

	class LLVMTupleCall : public TupleCallBody {
	private:
		std::unique_ptr<CompiledLLVM> m_compiled;
		LLVMCallFN m_call;

	public:
		LLVMTupleCall(std::unique_ptr<CompiledLLVM> compiled)
			: m_compiled(std::move(compiled))
		{
			m_call = (LLVMCallFN)m_compiled->function_ptr();
		}

		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			fn_out.destruct_all();
			BLI_assert(fn_in.all_initialized());
			BLI_assert(fn_out.all_uninitialized());

			m_call(
				fn_in.data_ptr(),
				fn_in.offsets_ptr(),
				fn_out.data_ptr(),
				fn_out.offsets_ptr());

			fn_in.set_all_uninitialized();
			fn_out.set_all_initialized();
		}
	};

	static TupleCallBody *compile_ir_to_tuple_call(
		SharedFunction &fn,
		llvm::LLVMContext &context,
		BuildIRFunction build_ir)
	{
		llvm::Module *module = new llvm::Module(fn->name(), context);
		llvm::Function *function = insert_tuple_call_function(fn, build_ir, module);

		auto compiled = CompiledLLVM::FromIR(module, function);
		return new LLVMTupleCall(std::move(compiled));
	}

	static TupleCallBody *build_from_compiled(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		auto *body = fn->body<LLVMCompiledBody>();
		return compile_ir_to_tuple_call(fn, context, [body](
				CodeBuilder &builder,
				CodeInterface &interface,
				const BuildIRSettings &settings)
			{
				body->build_ir(builder, interface, settings);
			});
	}

	static TupleCallBody *build_from_ir_generator(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		auto *body = fn->body<LLVMBuildIRBody>();
		return compile_ir_to_tuple_call(fn, context, [body](
				CodeBuilder &builder,
				CodeInterface &interface,
				const BuildIRSettings &settings)
			{
				body->build_ir(builder, interface, settings);
			});
	}

	void derive_TupleCallBody_from_LLVMBuildIRBody(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		BLI_assert(fn->has_body<LLVMBuildIRBody>());
		BLI_assert(!fn->has_body<TupleCallBody>());

		fn->add_body(build_from_ir_generator(fn, context));
	}

	void derive_TupleCallBody_from_LLVMCompiledBody(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		BLI_assert(fn->has_body<LLVMCompiledBody>());
		BLI_assert(!fn->has_body<TupleCallBody>());

		fn->add_body(build_from_compiled(fn, context));
	}

} /* namespace FN */