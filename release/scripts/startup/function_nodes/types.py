from . types_base import (
    DataTypesInfo,
    UniqueSocketBuilder,
    ColoredSocketBuilder,
)

from . sockets import (
    FloatSocket,
    IntegerSocket,
    VectorSocket,
    BooleanSocket,
    CustomColoredSocket,
)

type_infos = DataTypesInfo()

type_infos.insert_data_type("Float", UniqueSocketBuilder(FloatSocket))
type_infos.insert_data_type("Vector", UniqueSocketBuilder(VectorSocket))
type_infos.insert_data_type("Integer", UniqueSocketBuilder(IntegerSocket))
type_infos.insert_data_type("Boolean", UniqueSocketBuilder(BooleanSocket))
type_infos.insert_data_type("Float List", ColoredSocketBuilder((0, 0.3, 0.5, 0.5)))
type_infos.insert_data_type("Vector List", ColoredSocketBuilder((0, 0, 0.5, 0.5)))
type_infos.insert_data_type("Integer List", ColoredSocketBuilder((0.3, 0.7, 0.5, 0.5)))

type_infos.insert_list_relation("Float", "Float List")
type_infos.insert_list_relation("Vector", "Vector List")
type_infos.insert_list_relation("Integer", "Integer List")

type_infos.insert_implicitly_convertable_types({"Float", "Integer"})
type_infos.insert_implicitly_convertable_types({"Float List", "Integer List"})
