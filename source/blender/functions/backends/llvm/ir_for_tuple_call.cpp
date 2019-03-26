#include "ir_for_tuple_call.hpp"
#include "llvm_types.hpp"
#include "FN_tuple_call.hpp"
#include "ir_utils.hpp"

namespace FN {

	static void run_TupleCallBody(
		TupleCallBody *body,
		SharedTupleMeta *meta_in_,
		SharedTupleMeta *meta_out_,
		void *data_in,
		void *data_out)
	{
		SharedTupleMeta &meta_in = *meta_in_;
		SharedTupleMeta &meta_out = *meta_out_;

		bool *initialized_in = (bool *)alloca(meta_in->element_amount());
		bool *initialized_out = (bool *)alloca(meta_out->element_amount());

		Tuple fn_in(meta_in, data_in, initialized_in, false);
		Tuple fn_out(meta_out, data_out, initialized_out, false);

		fn_in.set_all_initialized();

		ExecutionStack stack;
		ExecutionContext ctx(stack);
		body->call(fn_in, fn_out, ctx);

		/* This way the data is not freed with the tuples. */
		fn_out.set_all_uninitialized();
	}

	class TupleCallLLVM : public LLVMBuildIRBody {
	private:
		TupleCallBody *m_tuple_call;
		SharedTupleMeta m_in_meta;
		SharedTupleMeta m_out_meta;

	public:
		TupleCallLLVM(TupleCallBody *tuple_call)
			: m_tuple_call(tuple_call),
			  m_in_meta(SharedTupleMeta::New(m_tuple_call->owner()->signature().input_types())),
			  m_out_meta(SharedTupleMeta::New(m_tuple_call->owner()->signature().output_types()))
		{}

		void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const override
		{
			Function *fn = m_tuple_call->owner();
			llvm::LLVMContext &context = builder.getContext();

			/* Find relevant type information. */
			auto input_type_infos = fn->signature().input_extensions<LLVMTypeInfo>();
			auto output_type_infos = fn->signature().output_extensions<LLVMTypeInfo>();

			LLVMTypes input_types = types_of_values(inputs);
			LLVMTypes output_types;
			for (auto type_info : output_type_infos) {
				output_types.append(type_info->get_type(context));
			}

			/* Build wrapper function. */
			llvm::Type *wrapper_output_type = llvm::StructType::get(context, to_array_ref(output_types));

			llvm::FunctionType *wrapper_function_type = llvm::FunctionType::get(
				wrapper_output_type, to_array_ref(input_types), false);

			llvm::Function *wrapper_function = llvm::Function::Create(
				wrapper_function_type,
				llvm::GlobalValue::LinkageTypes::InternalLinkage,
				fn->name() + " Wrapper",
				builder.GetInsertBlock()->getModule());

			this->build_wrapper_function(
				wrapper_function,
				input_type_infos,
				output_type_infos,
				wrapper_output_type);

			/* Call wrapper function. */
			llvm::Value *output_struct = builder.CreateCall(
				wrapper_function, to_array_ref(const_cast<LLVMValues&>(inputs)));

			/* Extract output values. */
			for (uint i = 0; i < output_type_infos.size(); i++) {
				llvm::Value *out = builder.CreateExtractValue(output_struct, i);
				r_outputs.append(out);
			}
		}

	private:
		void build_wrapper_function(
			llvm::Function *function,
			SmallVector<LLVMTypeInfo *> &input_type_infos,
			SmallVector<LLVMTypeInfo *> &output_type_infos,
			llvm::Type *output_type) const
		{
			llvm::LLVMContext &context = function->getContext();

			llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
			llvm::IRBuilder<> builder(bb);

			/* Type declarations. */
			llvm::Type *void_ty = llvm::Type::getVoidTy(context);
			llvm::Type *void_ptr_ty = void_ty->getPointerTo();
			llvm::Type *byte_ptr_ty = llvm::Type::getInt8PtrTy(context);

			llvm::FunctionType *call_ftype = llvm::FunctionType::get(
				void_ty, {void_ptr_ty, void_ptr_ty, void_ptr_ty, byte_ptr_ty, byte_ptr_ty}, false);


			/* Allocate temporary stack buffer for tuple input and output. */
			llvm::Value *tuple_in_data_ptr = alloca_bytes(builder, m_in_meta->total_data_size());
			tuple_in_data_ptr->setName("tuple_in_data");
			llvm::Value *tuple_out_data_ptr = alloca_bytes(builder, m_out_meta->total_data_size());
			tuple_out_data_ptr->setName("tuple_out_data");

			llvm::Value *meta_in_ptr = void_ptr_to_ir(builder, (void *)&m_in_meta);
			llvm::Value *meta_out_ptr = void_ptr_to_ir(builder, (void *)&m_out_meta);

			/* Write input values into buffer. */
			for (uint i = 0; i < input_type_infos.size(); i++) {
				llvm::Value *arg = function->arg_begin() + i;
				llvm::Value *store_at_addr = builder.CreateConstGEP1_32(tuple_in_data_ptr, m_in_meta->offsets()[i]);
				input_type_infos[i]->build_store_ir__relocate(builder, arg, store_at_addr);
			}

			/* Execute tuple call body. */
			call_pointer(builder, (void *)run_TupleCallBody,
				call_ftype,
				{void_ptr_to_ir(builder, m_tuple_call),
				 meta_in_ptr,
				 meta_out_ptr,
				 tuple_in_data_ptr,
				 tuple_out_data_ptr});

			/* Read output values from buffer. */
			llvm::Value *output = llvm::UndefValue::get(output_type);
			for (uint i = 0; i < output_type_infos.size(); i++) {
				llvm::Value *load_from_addr = builder.CreateConstGEP1_32(tuple_out_data_ptr, m_out_meta->offsets()[i]);
				llvm::Value *out = output_type_infos[i]->build_load_ir__relocate(builder, load_from_addr);
				output = builder.CreateInsertValue(output, out, i);
			}

			builder.CreateRet(output);
		}
	};

	void derive_LLVMBuildIRBody_from_TupleCallBody(
		SharedFunction &fn)
	{
		BLI_assert(fn->has_body<TupleCallBody>());
		BLI_assert(!fn->has_body<LLVMBuildIRBody>());

		fn->add_body(new TupleCallLLVM(fn->body<TupleCallBody>()));
	}

} /* namespace FN */