#define LAZY_INIT_NO_ARG(final_ret_type, builder_ret_type, func_name) \
	builder_ret_type func_name##_impl(); \
	final_ret_type func_name() \
	{ \
		static builder_ret_type value = func_name##_impl(); \
		return value; \
	} \
	builder_ret_type func_name##_impl()
