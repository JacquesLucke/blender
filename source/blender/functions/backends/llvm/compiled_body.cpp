#include "FN_llvm.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace FN {

	BLI_COMPOSITION_IMPLEMENTATION(LLVMCompiledBody);

	void LLVMCompiledBody::build_ir(
		CodeBuilder &builder,
		CodeInterface &interface,
		const BuildIRSettings &UNUSED(settings)) const
	{
		auto *ftype = function_type_from_signature(
			this->owner()->signature(), builder.getContext());

		llvm::Value *output_struct = builder.CreateCallPointer(
			this->function_ptr(), ftype, interface.inputs());
		for (uint i = 0; i < ftype->getReturnType()->getStructNumElements(); i++) {
			llvm::Value *out = builder.CreateExtractValue(output_struct, i);
			interface.set_output(i, out);
		}
	}

	static LLVMCompiledBody *compile_body(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		auto input_type_infos = fn->signature().input_extensions<LLVMTypeInfo>();
		auto output_type_infos = fn->signature().output_extensions<LLVMTypeInfo>();
		LLVMTypes input_types = types_of_type_infos(input_type_infos, context);
		LLVMTypes output_types = types_of_type_infos(output_type_infos, context);

		llvm::Type *output_type = llvm::StructType::get(context, to_array_ref(output_types));

		llvm::FunctionType *function_type = llvm::FunctionType::get(
			output_type, to_array_ref(input_types), false);

		llvm::Module *module = new llvm::Module(fn->name(), context);

		llvm::Function *function = llvm::Function::Create(
			function_type,
			llvm::GlobalValue::LinkageTypes::ExternalLinkage,
			fn->name(),
			module);

		LLVMValues input_values;
		for (llvm::Value &value : function->args()) {
			input_values.append(&value);
		}

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
		CodeBuilder builder(bb);

		LLVMBuildIRBody *gen_body = fn->body<LLVMBuildIRBody>();
		BLI_assert(gen_body);

		LLVMValues output_values(output_types.size());
		BuildIRSettings settings;
		CodeInterface interface(input_values, output_values);
		gen_body->build_ir(builder, interface, settings);

		llvm::Value *return_value = llvm::UndefValue::get(output_type);
		for (uint i = 0; i < output_values.size(); i++) {
			return_value = builder.CreateInsertValue(return_value, output_values[i], i);
		}
		builder.CreateRet(return_value);

		auto compiled = CompiledLLVM::FromIR(module, function);
		return new LLVMCompiledBody(std::move(compiled));
	}

	void derive_LLVMCompiledBody_from_LLVMBuildIRBody(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		BLI_assert(fn->has_body<LLVMBuildIRBody>());
		BLI_assert(!fn->has_body<LLVMCompiledBody>());

		fn->add_body(compile_body(fn, context));
	}

} /* namespace FN */