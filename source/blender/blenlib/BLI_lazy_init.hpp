#define LAZY_INIT__NO_ARG(final_ret_type, builder_ret_type, func_name) \
	static builder_ret_type func_name##_impl(void); \
	final_ret_type func_name(void) \
	{ \
		static builder_ret_type value = func_name##_impl(); \
		return value; \
	} \
	builder_ret_type func_name##_impl(void)

#define LAZY_INIT_STATIC__NO_ARG(final_ret_type, builder_ret_type, func_name) \
	static final_ret_type func_name(void); \
	LAZY_INIT__NO_ARG(final_ret_type, builder_ret_type, func_name)

#define LAZY_INIT_REF__NO_ARG(type, func_name) \
	LAZY_INIT__NO_ARG(type &, type, func_name)

#define LAZY_INIT_REF_STATIC__NO_ARG(type, func_name) \
	LAZY_INIT_STATIC__NO_ARG(type &, type, func_name)
