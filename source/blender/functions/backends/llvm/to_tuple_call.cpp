#include "to_tuple_call.hpp"
#include "llvm_types.hpp"
#include "llvm_gen.hpp"
#include "ir_utils.hpp"

#include "FN_tuple_call.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace FN {

	static llvm::Function *insert_tuple_call_function(
		Function *fn,
		LLVMGenBody *llvm_body,
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
		llvm::IRBuilder<> builder(bb);

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
			llvm::Value *value = type_info->build_load_ir__copy(
				builder, value_byte_addr);

			input_values.append(value);
		}

		LLVMValues output_values;
		llvm_body->build_ir(builder, input_values, output_values);

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
		LLVMCallFN m_call;

	public:
		LLVMTupleCall(LLVMCallFN call)
			: m_call(call) {}

		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			BLI_assert(fn_in.all_initialized());

			m_call(
				fn_in.data_ptr(),
				fn_in.offsets_ptr(),
				fn_out.data_ptr(),
				fn_out.offsets_ptr());

			fn_out.set_all_initialized();
		}
	};

	TupleCallBody *compile_llvm_to_tuple_call(
		LLVMGenBody *llvm_body,
		llvm::LLVMContext &context)
	{
		BLI_assert(llvm_body->has_owner());
		Function *fn = llvm_body->owner();
		llvm::Module *module = new llvm::Module(fn->name(), context);
		llvm::Function *function = insert_tuple_call_function(fn, llvm_body, module);

		module->print(llvm::outs(), nullptr);

		llvm::verifyFunction(*function, &llvm::outs());
		llvm::verifyModule(*module, &llvm::outs());

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();

		llvm::ExecutionEngine *ee = llvm::EngineBuilder(
			std::unique_ptr<llvm::Module>(module)).create();
		ee->finalizeObject();
		ee->generateCodeForModule(module);

		uint64_t fn_ptr = ee->getFunctionAddress(
			function->getName().str());

		return new LLVMTupleCall((LLVMCallFN)fn_ptr);
	}

} /* namespace FN */